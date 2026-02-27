#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>

#include <git2.h>
#include <spdlog/spdlog.h>

#include "changelog.h"
#include "utils.h"
#include "version.h"

namespace {

struct GitRepoDeleter {
    void operator()(git_repository* r) const { git_repository_free(r); }
};

struct GitRevwalkDeleter {
    void operator()(git_revwalk* w) const { git_revwalk_free(w); }
};

struct GitCommitDeleter {
    void operator()(git_commit* c) const { git_commit_free(c); }
};

struct GitDiffDeleter {
    void operator()(git_diff* d) const { git_diff_free(d); }
};

struct GitTreeDeleter {
    void operator()(git_tree* t) const { git_tree_free(t); }
};

struct GitRemoteDeleter {
    void operator()(git_remote* r) const { git_remote_free(r); }
};

using UniqueCommit = std::unique_ptr<git_commit, GitCommitDeleter>;
using UniqueDiff = std::unique_ptr<git_diff, GitDiffDeleter>;
using UniqueTree = std::unique_ptr<git_tree, GitTreeDeleter>;
using UniqueRemote = std::unique_ptr<git_remote, GitRemoteDeleter>;

struct LibGit2Init {
    LibGit2Init() { git_libgit2_init(); }
    ~LibGit2Init() { git_libgit2_shutdown(); }
};

static LibGit2Init g_libgit2_init;

#define _CHECK_GIT2(error, msg)                                       \
    if (error < 0) {                                                  \
        const git_error* e = git_error_last();                        \
        throw std::runtime_error(std::string(msg) + ": " +            \
                                 (e ? e->message : "unknown error")); \
    }

}  // namespace

const std::map<CommitType, std::string>& CommitTypeNames() {
    static const std::map<CommitType, std::string> names = {
        {CommitType::kAdd, "Add"},           {CommitType::kFeat, "Feat"},
        {CommitType::kRefactor, "Refactor"}, {CommitType::kDeprecated, "Deprecated"},
        {CommitType::kFix, "Fix"},           {CommitType::kDocs, "Docs"},
        {CommitType::kTest, "Test"},         {CommitType::kPerf, "Perf"},
    };
    return names;
}

const std::map<std::string, CommitType>& PrefixToCommitType() {
    static const std::map<std::string, CommitType> prefixes = {
        {"add", CommitType::kAdd},           {"feat", CommitType::kFeat},
        {"refactor", CommitType::kRefactor}, {"deprecated", CommitType::kDeprecated},
        {"fix", CommitType::kFix},           {"docs", CommitType::kDocs},
        {"test", CommitType::kTest},         {"perf", CommitType::kPerf},
    };
    return prefixes;
}

Changelog::Changelog(Config config) : config_(std::move(config)) {
    _CHECK_GIT2(git_repository_open(&repo_, config_.repo.c_str()),
                "Failed to open repository at " + config_.repo);
    if (config_.url.empty()) {
        git_remote* remote_raw = nullptr;
        int e = git_remote_lookup(&remote_raw, repo_, "origin");
        UniqueRemote remote(remote_raw);
        if (e == 0) {
            config_.url = std::string(git_remote_url(remote.get()));
        } else if (e == GIT_ENOTFOUND) {
            spdlog::error("Repository {} not found.", config_.repo);
            std::exit(e);
        } else if (e == GIT_EINVALIDSPEC) {
            spdlog::error("ref/spec was not in valid format.");
            std::exit(e);
        }
    }
    if (config_.url.compare(0, kSSHPrefix.length(), kSSHPrefix) == 0) {
        config_.url = this->SSH2HTTPS(config_.url);
    }
    config_.repo_name = config_.url;
    std::vector<std::string> comps = split(config_.url, "/");
    config_.repo_name = comps[comps.size() - 1];
}

Changelog::~Changelog() {
    if (repo_) {
        git_repository_free(repo_);
    }
}

std::string Changelog::SSH2HTTPS(const std::string url) {
    std::size_t end = config_.url.length();
    end = end - kSSHPrefix.length() - kSSHSuffix.length();
    std::string https = url.substr(kSSHPrefix.length(), end);
    https = kHTTPSPrefix + https;
    return https;
}

std::string Changelog::ShortHash(const git_oid* oid) {
    char buf[8] = {};
    git_oid_tostr(buf, sizeof(buf), oid);
    return std::string(buf);
}

std::string Changelog::FullHash(const git_oid* oid) {
    char buf[GIT_OID_SHA1_HEXSIZE + 1] = {};
    git_oid_tostr(buf, sizeof(buf), oid);
    return std::string(buf);
}

std::string Changelog::FormatDate(git_time_t time) {
    std::time_t t = static_cast<std::time_t>(time);
    std::tm tm = {};
    gmtime_r(&t, &tm);
    char buf[11] = {};
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return std::string(buf);
}

bool Changelog::IsBreakingChange(const std::string& summary) {
    auto colon_pos = summary.find(':');
    if (colon_pos == std::string::npos || colon_pos == 0) {
        return false;
    }
    return summary[colon_pos - 1] == '!';
}

std::optional<CommitType> Changelog::CategorizeCommit(const std::string& summary) {
    auto colon_pos = summary.find(':');
    if (colon_pos == std::string::npos) {
        return std::nullopt;
    }

    std::string prefix = summary.substr(0, colon_pos);
    // Convert to lowercase for matching.
    std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::tolower);
    // Strip scope like "fix(core)" -> "fix"
    auto paren_pos = prefix.find('(');
    if (paren_pos != std::string::npos) {
        prefix = prefix.substr(0, paren_pos);
    }
    // Strip breaking-change marker: "feat!" -> "feat"
    if (!prefix.empty() && prefix.back() == '!') {
        prefix.pop_back();
    }

    const auto& prefixes = PrefixToCommitType();
    auto it = prefixes.find(prefix);
    if (it != prefixes.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::string Changelog::FormatEntry(const std::string& summary, const git_oid* oid,
                                   const git_signature* author) {
    std::string short_hash = ShortHash(oid);
    std::string full_hash = FullHash(oid);
    return summary + " by " + author->name + " in [#" + short_hash + "](" +
           config_.url + "/commit/" + full_hash + ")";
}

bool Changelog::CommitTouchesPath(git_commit* commit, const std::string& path) const {
    git_tree* commit_tree_raw = nullptr;
    _CHECK_GIT2(git_commit_tree(&commit_tree_raw, commit), "Failed to get tree");
    UniqueTree commit_tree(commit_tree_raw);

    UniqueTree parent_tree;
    if (git_commit_parentcount(commit) > 0) {
        git_commit* parent_raw = nullptr;
        _CHECK_GIT2(git_commit_parent(&parent_raw, commit, 0), "Failed to get parent");
        UniqueCommit parent(parent_raw);

        git_tree* parent_tree_raw = nullptr;
        _CHECK_GIT2(git_commit_tree(&parent_tree_raw, parent.get()),
                    "Failed to get parent tree");
        parent_tree.reset(parent_tree_raw);
    }

    git_diff_options opts = {};
    git_diff_options_init(&opts, GIT_DIFF_OPTIONS_VERSION);
    char* pathspec = const_cast<char*>(path.c_str());
    opts.pathspec.strings = &pathspec;
    opts.pathspec.count = 1;

    git_diff* diff_raw = nullptr;
    _CHECK_GIT2(git_diff_tree_to_tree(&diff_raw, repo_, parent_tree.get(),
                                      commit_tree.get(), &opts),
                "Failed to diff trees");
    UniqueDiff diff(diff_raw);

    return git_diff_num_deltas(diff.get()) > 0;
}

SectionData Changelog::GetGitLogs(const std::string& follow_path) {
    SectionData data;

    git_revwalk* walker_raw = nullptr;
    _CHECK_GIT2(git_revwalk_new(&walker_raw, repo_), "Failed to create revwalk");
    std::unique_ptr<git_revwalk, GitRevwalkDeleter> walker(walker_raw);

    _CHECK_GIT2(git_revwalk_push_head(walker.get()), "Failed to push HEAD");
    git_revwalk_sorting(walker.get(), GIT_SORT_TIME);

    git_oid oid;
    while (git_revwalk_next(&oid, walker.get()) == 0) {
        git_commit* commit_raw = nullptr;
        _CHECK_GIT2(git_commit_lookup(&commit_raw, repo_, &oid),
                    "Failed to lookup commit");
        UniqueCommit commit(commit_raw);

        if (!follow_path.empty() && !CommitTouchesPath(commit.get(), follow_path)) {
            continue;
        }

        const char* summary = git_commit_summary(commit.get());
        if (!summary) continue;

        if (IsBreakingChange(summary)) {
            data.has_breaking_change = true;
        }

        auto type = CategorizeCommit(summary);
        if (!type) continue;

        std::string entry = FormatEntry(summary, &oid, git_commit_author(commit.get()));
        data.entries[*type].insert(entry);

        spdlog::debug("{} -> {}", CommitTypeNames().at(*type), entry);
    }

    return data;
}

SemanticVersion Changelog::DetectInitialVersion() const {
    git_strarray tags = {};
    int err = git_tag_list(&tags, repo_);
    if (err < 0) {
        spdlog::debug("No tags found, using default v0.1.0");
        return {0, 1, 0};
    }

    SemanticVersion highest = {0, 0, 0};
    bool found_any = false;

    for (size_t i = 0; i < tags.count; ++i) {
        std::string tag_name(tags.strings[i]);
        try {
            SemanticVersion v = SemanticVersion::Parse(tag_name);
            if (!found_any || highest < v) {
                highest = v;
                found_any = true;
            }
        } catch (...) {
            continue;
        }
    }

    git_strarray_free(&tags);

    if (!found_any) {
        spdlog::debug("No semver tags found, using default v0.1.0");
        return {0, 1, 0};
    }

    spdlog::debug("Detected latest version from tags: {}", highest.ToString());
    return highest;
}

std::string Changelog::FormatChangelog(
    const std::vector<std::pair<std::string, SectionData>>& sections,
    const std::string& date) {
    std::ostringstream out;
    const auto& type_names = CommitTypeNames();

    for (const auto& [section_name, data] : sections) {
        out << "## " << section_name << " \u2014 " << date << "\n\n";

        for (const auto& [type, logs] : data.entries) {
            if (logs.empty()) continue;
            out << "### " << type_names.at(type) << "\n\n";
            for (const auto& log : logs) {
                out << "- " << log << "\n";
            }
            out << "\n";
        }
    }

    return out.str();
}

std::string Changelog::ReadChangelogFile(const std::string& fpath) {
    std::ifstream file(fpath);
    if (!file.is_open()) {
        return "";
    }

    std::string content;
    std::string line;
    bool first_line = true;
    while (std::getline(file, line)) {
        if (first_line) {
            first_line = false;
            // Skip the "# Changelog" header line.
            if (line.find("# Changelog") == 0) continue;
        }
        content += line + "\n";
    }

    return content;
}

std::vector<ParsedSection> Changelog::ParseChangelogStructured(
    const std::string& content) {
    std::vector<ParsedSection> sections;

    ParsedSection* cur = nullptr;
    std::optional<CommitType> cur_type;

    // Match both old and new formats:
    //   ## repo_name — YYYY-MM-DD
    //   ## repo_name@vX.Y.Z — YYYY-MM-DD
    std::regex section_re(
        R"(^## (.+?)(?:@(v\d+\.\d+\.\d+))?\s+(?:--|—)\s+(\d{4}-\d{2}-\d{2})$)");
    std::regex type_re(R"(^### (\w+)$)");
    std::regex entry_re(R"(^- (.+)$)");

    std::istringstream stream(content);
    std::string line;
    const auto& prefixes = PrefixToCommitType();

    while (std::getline(stream, line)) {
        std::smatch match;

        if (std::regex_match(line, match, section_re)) {
            sections.emplace_back();
            cur = &sections.back();
            cur->name = match[1].str();
            if (match[2].matched) {
                cur->version = SemanticVersion::Parse(match[2].str());
            }
            cur->date = match[3].str();
            cur_type = std::nullopt;
        } else if (std::regex_match(line, match, type_re)) {
            std::string type_str = match[1].str();
            std::transform(type_str.begin(), type_str.end(), type_str.begin(),
                           ::tolower);
            auto it = prefixes.find(type_str);
            cur_type =
                (it != prefixes.end()) ? std::optional(it->second) : std::nullopt;
        } else if (std::regex_match(line, match, entry_re) && cur_type && cur) {
            std::string entry_text = match[1].str();
            cur->entries[*cur_type].insert(entry_text);
            // Detect breaking change from preserved commit summary.
            if (entry_text.find("!:") != std::string::npos) {
                cur->has_breaking_change = true;
            }
        }
    }

    return sections;
}

std::set<std::string> Changelog::FlattenEntries(
    const std::vector<ParsedSection>& sections) {
    std::set<std::string> all;
    for (const auto& sec : sections) {
        for (const auto& [type, logs] : sec.entries) {
            all.insert(logs.begin(), logs.end());
        }
    }
    return all;
}

SectionData Changelog::FilterNewEntries(const SectionData& current,
                                        const std::set<std::string>& existing_entries) {
    SectionData result;
    for (const auto& [type, logs] : current.entries) {
        for (const auto& log : logs) {
            if (existing_entries.find(log) == existing_entries.end()) {
                result.entries[type].insert(log);
                // Recompute breaking-change flag from filtered entries only.
                if (log.find("!:") != std::string::npos) {
                    result.has_breaking_change = true;
                }
            }
        }
    }
    return result;
}

void Changelog::Generate() {
    // Get today's date.
    auto now = std::chrono::system_clock::now();
    std::time_t now_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = {};
    gmtime_r(&now_t, &tm);
    char date_buf[11] = {};
    std::strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &tm);
    std::string today(date_buf);

    // Collect current git logs.
    std::map<std::string, SectionData> current_sections;
    if (config_.follow.empty()) {
        spdlog::debug("Getting logs for entire repository");
        current_sections[config_.repo_name] = GetGitLogs();
    } else {
        for (const auto& path : config_.follow) {
            spdlog::debug("Getting logs for path: {}", path);
            current_sections[path] = GetGitLogs(path);
        }
    }

    // Read and parse existing changelog.
    std::string existing_raw = ReadChangelogFile(config_.output);
    auto existing_sections = ParseChangelogStructured(existing_raw);
    std::set<std::string> existing_flat = FlattenEntries(existing_sections);

    // Filter out already-recorded entries.
    std::map<std::string, SectionData> new_sections;
    for (auto& [name, data] : current_sections) {
        SectionData filtered = FilterNewEntries(data, existing_flat);
        if (!filtered.entries.empty()) {
            new_sections[name] = std::move(filtered);
        }
    }

    // Detect initial version from git tags.
    SemanticVersion seed = DetectInitialVersion();

    // Determine the last version from existing sections.
    SemanticVersion last_version = seed;
    bool needs_backfill = false;

    if (!existing_sections.empty() && existing_sections.front().version) {
        last_version = *existing_sections.front().version;
    } else if (!existing_sections.empty()) {
        needs_backfill = true;
    }

    // Backfill versions on old unversioned sections.
    std::string existing_content;
    if (needs_backfill) {
        const auto& type_names = CommitTypeNames();
        std::ostringstream oss;

        // Assign the detected tag version to old unversioned sections.
        // We can't accurately reconstruct per-section versions from
        // changelog text alone, so they all get the tag version.
        for (auto& sec : existing_sections) {
            sec.version = seed;
        }
        last_version = seed;

        // Re-format existing content with versions.
        for (const auto& sec : existing_sections) {
            oss << "## " << sec.name << "@" << sec.version->ToString() << " \u2014 "
                << sec.date << "\n\n";
            for (const auto& [type, logs] : sec.entries) {
                if (logs.empty()) continue;
                oss << "### " << type_names.at(type) << "\n\n";
                for (const auto& log : logs) {
                    oss << "- " << log << "\n";
                }
                oss << "\n";
            }
        }
        existing_content = oss.str();
    } else {
        existing_content = existing_raw;
    }

    // Compute version for the new section(s).
    // If no existing sections, use the seed version directly (first release).
    bool first_release = existing_sections.empty();
    std::vector<std::pair<std::string, SectionData>> new_versioned;
    for (auto& [name, data] : new_sections) {
        SemanticVersion new_ver;
        if (first_release) {
            new_ver = seed;
            first_release = false;
        } else {
            std::set<CommitType> types;
            for (const auto& [type, _] : data.entries) {
                types.insert(type);
            }
            new_ver = ComputeNextVersion(last_version, types, data.has_breaking_change);
        }
        std::string versioned_name = name + "@" + new_ver.ToString();
        new_versioned.emplace_back(versioned_name, std::move(data));
        last_version = new_ver;
    }

    // Format and write.
    std::string new_markdown = FormatChangelog(new_versioned, today);

    std::ofstream out(config_.output);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot open output file: " + config_.output);
    }

    out << "# Changelog\n\n";
    if (!new_markdown.empty()) {
        out << new_markdown;
    }
    if (!existing_content.empty()) {
        out << existing_content;
    }

    spdlog::info("Wrote changelog to: {}", config_.output);
}

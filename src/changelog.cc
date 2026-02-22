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

void CheckGit2(int error, const std::string& message) {
    if (error < 0) {
        const git_error* e = git_error_last();
        throw std::runtime_error(message + ": " + (e ? e->message : "unknown error"));
    }
}

}  // namespace

const std::map<CommitType, std::string>& CommitTypeNames() {
    static const std::map<CommitType, std::string> names = {
        {CommitType::kAdd, "Add"},           {CommitType::kFeat, "Feat"},
        {CommitType::kRefactor, "Refactor"}, {CommitType::kDeprecated, "Deprecated"},
        {CommitType::kFix, "Fix"},           {CommitType::kDocs, "Docs"},
        {CommitType::kTest, "Test"},
    };
    return names;
}

const std::map<std::string, CommitType>& PrefixToCommitType() {
    static const std::map<std::string, CommitType> prefixes = {
        {"add", CommitType::kAdd},           {"feat", CommitType::kFeat},
        {"refactor", CommitType::kRefactor}, {"deprecated", CommitType::kDeprecated},
        {"fix", CommitType::kFix},           {"docs", CommitType::kDocs},
        {"test", CommitType::kTest},
    };
    return prefixes;
}

Changelog::Changelog(Config config) : config_(std::move(config)) {
    CheckGit2(git_repository_open(&repo_, config_.repo.c_str()),
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

    const auto& prefixes = PrefixToCommitType();
    auto it = prefixes.find(prefix);
    if (it != prefixes.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::string Changelog::FormatEntry(const std::string& summary, const git_oid* oid) {
    std::string short_hash = ShortHash(oid);
    std::string full_hash = FullHash(oid);
    return summary + " ([#" + short_hash + "](" + config_.url + "/commit/" + full_hash +
           "))";
}

bool Changelog::CommitTouchesPath(git_commit* commit, const std::string& path) const {
    git_tree* commit_tree_raw = nullptr;
    CheckGit2(git_commit_tree(&commit_tree_raw, commit), "Failed to get tree");
    UniqueTree commit_tree(commit_tree_raw);

    UniqueTree parent_tree;
    if (git_commit_parentcount(commit) > 0) {
        git_commit* parent_raw = nullptr;
        CheckGit2(git_commit_parent(&parent_raw, commit, 0), "Failed to get parent");
        UniqueCommit parent(parent_raw);

        git_tree* parent_tree_raw = nullptr;
        CheckGit2(git_commit_tree(&parent_tree_raw, parent.get()),
                  "Failed to get parent tree");
        parent_tree.reset(parent_tree_raw);
    }

    git_diff_options opts = {};
    git_diff_options_init(&opts, GIT_DIFF_OPTIONS_VERSION);
    char* pathspec = const_cast<char*>(path.c_str());
    opts.pathspec.strings = &pathspec;
    opts.pathspec.count = 1;

    git_diff* diff_raw = nullptr;
    CheckGit2(git_diff_tree_to_tree(&diff_raw, repo_, parent_tree.get(),
                                    commit_tree.get(), &opts),
              "Failed to diff trees");
    UniqueDiff diff(diff_raw);

    return git_diff_num_deltas(diff.get()) > 0;
}

SectionEntries Changelog::GetGitLogs(const std::string& follow_path) {
    SectionEntries entries;

    git_revwalk* walker_raw = nullptr;
    CheckGit2(git_revwalk_new(&walker_raw, repo_), "Failed to create revwalk");
    std::unique_ptr<git_revwalk, GitRevwalkDeleter> walker(walker_raw);

    CheckGit2(git_revwalk_push_head(walker.get()), "Failed to push HEAD");
    git_revwalk_sorting(walker.get(), GIT_SORT_TIME);

    git_oid oid;
    while (git_revwalk_next(&oid, walker.get()) == 0) {
        git_commit* commit_raw = nullptr;
        CheckGit2(git_commit_lookup(&commit_raw, repo_, &oid),
                  "Failed to lookup commit");
        UniqueCommit commit(commit_raw);

        if (!follow_path.empty() && !CommitTouchesPath(commit.get(), follow_path)) {
            continue;
        }

        const char* summary = git_commit_summary(commit.get());
        if (!summary) continue;

        auto type = CategorizeCommit(summary);
        if (!type) continue;

        std::string entry = FormatEntry(summary, &oid);
        entries[*type].insert(entry);

        spdlog::debug("{} -> {}", CommitTypeNames().at(*type), entry);
    }

    return entries;
}

std::string Changelog::FormatChangelog(const ChangelogEntries& entries,
                                       const std::string& date) {
    std::ostringstream out;
    const auto& type_names = CommitTypeNames();

    for (const auto& [section, section_entries] : entries) {
        std::string display_section = section.empty() ? "All Changes" : section;
        out << "## " << display_section << " \u2014 " << date << "\n\n";

        for (const auto& [type, logs] : section_entries) {
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

ChangelogEntries Changelog::ParseChangelog(const std::string& content) {
    ChangelogEntries entries;

    std::string cur_section;
    std::optional<CommitType> cur_type;

    // Match: ## Section — YYYY-MM-DD  or  ## Section -- YYYY-MM-DD
    std::regex section_re(R"(^## (.+?)\s+(?:--|—)\s+(\d{4}-\d{2}-\d{2})$)");
    // Match: ### TypeName
    std::regex type_re(R"(^### (\w+)$)");
    // Match: - entry text
    std::regex entry_re(R"(^- (.+)$)");

    std::istringstream stream(content);
    std::string line;
    const auto& prefixes = PrefixToCommitType();

    while (std::getline(stream, line)) {
        std::smatch match;

        if (std::regex_match(line, match, section_re)) {
            cur_section = match[1].str();
            cur_type = std::nullopt;
        } else if (std::regex_match(line, match, type_re)) {
            std::string type_str = match[1].str();
            std::transform(type_str.begin(), type_str.end(), type_str.begin(),
                           ::tolower);
            auto it = prefixes.find(type_str);
            if (it != prefixes.end()) {
                cur_type = it->second;
            } else {
                cur_type = std::nullopt;
            }
        } else if (std::regex_match(line, match, entry_re) && cur_type) {
            entries[cur_section][*cur_type].insert(match[1].str());
        }
    }

    return entries;
}

ChangelogEntries Changelog::DiffEntries(const ChangelogEntries& current,
                                        const ChangelogEntries& existing) {
    ChangelogEntries result;

    for (const auto& [section, section_entries] : current) {
        for (const auto& [type, logs] : section_entries) {
            std::set<std::string> new_logs;

            auto existing_section = existing.find(section);
            if (existing_section != existing.end()) {
                auto existing_type = existing_section->second.find(type);
                if (existing_type != existing_section->second.end()) {
                    std::set_difference(logs.begin(), logs.end(),
                                        existing_type->second.begin(),
                                        existing_type->second.end(),
                                        std::inserter(new_logs, new_logs.begin()));
                } else {
                    new_logs = logs;
                }
            } else {
                new_logs = logs;
            }

            if (!new_logs.empty()) {
                result[section][type] = std::move(new_logs);
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

    // Collect git logs.
    ChangelogEntries current;
    if (config_.follow.empty()) {
        spdlog::debug("Getting logs for entire repository");
        current[""] = GetGitLogs();
    } else {
        for (const auto& path : config_.follow) {
            spdlog::debug("Getting logs for path: {}", path);
            current[path] = GetGitLogs(path);
        }
    }

    // Read and parse existing changelog.
    std::string existing_content = ReadChangelogFile(config_.output);
    ChangelogEntries existing = ParseChangelog(existing_content);

    // Compute new entries.
    ChangelogEntries new_entries = DiffEntries(current, existing);

    // Format and write.
    std::string new_markdown = FormatChangelog(new_entries, today);

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

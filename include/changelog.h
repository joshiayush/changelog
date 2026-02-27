#ifndef CHANGELOG_H_
#define CHANGELOG_H_

#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <git2.h>

#include "version.h"

enum class CommitType {
    kAdd,
    kFeat,
    kRefactor,
    kDeprecated,
    kFix,
    kDocs,
    kTest,
    kPerf,
};

const std::map<CommitType, std::string>& CommitTypeNames();
const std::map<std::string, CommitType>& PrefixToCommitType();

struct CommitEntry {
    const std::string summary;
    const git_oid oid;
    const std::string author_name;

    bool operator<(const CommitEntry& o) const { return summary < o.summary; }
};

// commit_type -> set<formatted_entry>
using SectionEntries = std::map<CommitType, std::set<CommitEntry>>;

struct SectionData {
    SectionEntries entries;
    bool has_breaking_change = false;
};

struct ParsedSection {
    std::string name;
    std::optional<SemanticVersion> version;
    std::string date;
    SectionEntries entries;
    bool has_breaking_change = false;
};

const std::string kSSHPrefix = "git@github.com:";
const std::string kSSHSuffix = ".git";
const std::string kHTTPSPrefix = "https://github.com/";

class Changelog {
   public:
    struct Config {
        std::string repo = ".";
        std::string output = "CHANGELOG.md";
        std::string repo_name;
        std::string url;
        std::vector<std::string> follow;
    };

    explicit Changelog(Config config);
    ~Changelog();

    Changelog(const Changelog&) = delete;
    Changelog& operator=(const Changelog&) = delete;

    void Generate();

   private:
    SectionData GetGitLogs(const std::string& follow = "");

    std::string FormatChangelog(
        const std::vector<std::pair<std::string, SectionData>>& sections,
        const std::string& date);

    static std::string ReadChangelogFile(const std::string& fpath);

    static std::vector<ParsedSection> ParseChangelogStructured(
        const std::string& content);

    static std::set<CommitEntry> FlattenEntries(
        const std::vector<ParsedSection>& sections);

    static SectionData FilterNewEntries(const SectionData& current,
                                        const std::set<CommitEntry>& existing_entries);

    static std::optional<CommitType> CategorizeCommit(const std::string& summary);
    static bool IsBreakingChange(const std::string& summary);

    static std::string ShortHash(const git_oid* oid);
    static std::string FullHash(const git_oid* oid);
    static std::string FormatDate(git_time_t time);

    SemanticVersion DetectInitialVersion() const;

    bool CommitTouchesPath(git_commit* commit, const std::string& path) const;

    std::string SSH2HTTPS(const std::string url);
    std::string FormatEntry(const CommitEntry& entry);

    Config config_;
    git_repository* repo_ = nullptr;
};

#endif  // CHANGELOG_H_

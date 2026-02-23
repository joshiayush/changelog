#ifndef CHANGELOG_H_
#define CHANGELOG_H_

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <git2.h>

enum class CommitType {
    kAdd,
    kFeat,
    kRefactor,
    kDeprecated,
    kFix,
    kDocs,
    kTest,
};

const std::map<CommitType, std::string>& CommitTypeNames();
const std::map<std::string, CommitType>& PrefixToCommitType();

// section_name -> { commit_type -> set<formatted_entry> }
using SectionEntries = std::map<CommitType, std::set<std::string>>;

// Top-level: section_name -> SectionEntries
using ChangelogEntries = std::map<std::string, SectionEntries>;

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
    SectionEntries GetGitLogs(const std::string& follow = "");

    std::string FormatChangelog(const ChangelogEntries& entries,
                                const std::string& date);

    static std::string ReadChangelogFile(const std::string& fpath);

    static ChangelogEntries ParseChangelog(const std::string& content);

    static ChangelogEntries DiffEntries(const ChangelogEntries& current,
                                        const ChangelogEntries& existing);

    static std::optional<CommitType> CategorizeCommit(const std::string& summary);

    static std::string ShortHash(const git_oid* oid);
    static std::string FullHash(const git_oid* oid);
    static std::string FormatDate(git_time_t time);

    bool CommitTouchesPath(git_commit* commit, const std::string& path) const;

    std::string SSH2HTTPS(const std::string url);
    std::string FormatEntry(const std::string& summary, const git_oid* oid);

    Config config_;
    git_repository* repo_ = nullptr;
};

#endif  // CHANGELOG_H_

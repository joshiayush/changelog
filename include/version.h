#ifndef CHANGELOG_VERSION_H_
#define CHANGELOG_VERSION_H_

#include <set>
#include <string>

enum class CommitType;

struct SemanticVersion {
    int major = 0;
    int minor = 1;
    int patch = 0;

    std::string ToString() const;
    static SemanticVersion Parse(const std::string& str);

    bool operator==(const SemanticVersion& o) const;
    bool operator<(const SemanticVersion& o) const;
};

// Compute the next version from a base, given the set of commit types
// present and whether any breaking changes exist.
//
// - Breaking change: +1 MAJOR (resets minor + patch)
// - Each distinct MINOR type (feat, add): +1 MINOR (resets patch)
// - Each distinct PATCH type (fix, perf, refactor): +1 PATCH
// - docs, test, deprecated: no bump
SemanticVersion ComputeNextVersion(const SemanticVersion& base,
                                   const std::set<CommitType>& types,
                                   bool has_breaking_change);

#endif  // CHANGELOG_VERSION_H_

#include <regex>
#include <stdexcept>

#include "changelog.h"
#include "version.h"

std::string SemanticVersion::ToString() const {
    return "v" + std::to_string(major) + "." + std::to_string(minor) + "." +
           std::to_string(patch);
}

SemanticVersion SemanticVersion::Parse(const std::string& str) {
    std::regex re(R"(^v?(\d+)\.(\d+)\.(\d+)$)");
    std::smatch m;
    if (!std::regex_match(str, m, re)) {
        throw std::runtime_error("Invalid version string: " + str);
    }
    return {std::stoi(m[1]), std::stoi(m[2]), std::stoi(m[3])};
}

bool SemanticVersion::operator==(const SemanticVersion& o) const {
    return major == o.major && minor == o.minor && patch == o.patch;
}

bool SemanticVersion::operator<(const SemanticVersion& o) const {
    if (major != o.major) return major < o.major;
    if (minor != o.minor) return minor < o.minor;
    return patch < o.patch;
}

SemanticVersion ComputeNextVersion(const SemanticVersion& base,
                                   const std::set<CommitType>& types,
                                   bool has_breaking_change) {
    SemanticVersion v = base;

    if (has_breaking_change) {
        v.major++;
        v.minor = 0;
        v.patch = 0;
        return v;
    }

    bool has_minor = false, has_patch = false;
    for (auto t : types) {
        if (t == CommitType::kFeat || t == CommitType::kAdd) has_minor = true;
        if (t == CommitType::kFix || t == CommitType::kPerf ||
            t == CommitType::kRefactor)
            has_patch = true;
    }

    if (has_minor) {
        v.minor++;
        v.patch = 0;
    } else if (has_patch) {
        v.patch++;
    }

    return v;
}

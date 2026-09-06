#include "VerseFormatter.h"

namespace VerseFormatter {
    namespace {
        // Replaces every occurrence, not just the first: "{text} ({text})" is a
        // reasonable template and the previous single-shot replace left the
        // second copy as a literal token in the pasted output.
        void ReplaceAll(std::string& subject, const std::string& token, const std::string& value) {
            if (token.empty()) return;
            size_t pos = subject.find(token);
            while (pos != std::string::npos) {
                subject.replace(pos, token.size(), value);
                pos = subject.find(token, pos + value.size());
            }
        }
    }

    std::string ComposeReplacementText(const VerseLinkConfig& config,
                                       const std::string& reference,
                                       const std::string& verseText) {
        if (!config.includeReferenceInReplacement || reference.empty()) {
            return verseText;
        }

        if (config.referenceOnFirstLine) {
            return reference + "\n" + verseText;
        }

        const std::string referenceToken = "{reference}";
        const std::string textToken = "{text}";

        std::string result = config.replacementFormat;
        const bool hasReference = result.find(referenceToken) != std::string::npos;
        const bool hasText = result.find(textToken) != std::string::npos;

        if (!hasReference && !hasText) {
            // A template that names neither token cannot say where anything goes;
            // emitting it verbatim would paste the template instead of the verse.
            return reference + " " + verseText;
        }

        ReplaceAll(result, referenceToken, reference);
        ReplaceAll(result, textToken, verseText);
        return result;
    }
}

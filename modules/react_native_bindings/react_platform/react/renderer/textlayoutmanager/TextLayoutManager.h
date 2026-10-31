#pragma once

#include <react/renderer/attributedstring/AttributedStringBox.h>
#include <react/renderer/attributedstring/ParagraphAttributes.h>
#include <react/renderer/core/LayoutConstraints.h>
#include <react/renderer/textlayoutmanager/TextLayoutContext.h>
#include <react/renderer/textlayoutmanager/TextMeasureCache.h>
#include <react/utils/ContextContainer.h>

#include <memory>

class RNTextShaper;

namespace facebook::react {

// Upstream's key ignores which surface the text belongs to, and so the theme
// it is measured with.
struct RNTextMeasureCacheKey {
	TextMeasureCacheKey text;
	uint64_t theme_revision = 0;

	bool operator==(const RNTextMeasureCacheKey &) const = default;
};

} // namespace facebook::react

template <>
struct std::hash<facebook::react::RNTextMeasureCacheKey> {
	size_t operator()(const facebook::react::RNTextMeasureCacheKey &p_key) const {
		return facebook::react::hash_combine(p_key.text, p_key.theme_revision);
	}
};

namespace facebook::react {

class TextLayoutManager {
public:
	explicit TextLayoutManager(const std::shared_ptr<const ContextContainer> &contextContainer);
	virtual ~TextLayoutManager() = default;

	TextLayoutManager(const TextLayoutManager &) = delete;
	TextLayoutManager &operator=(const TextLayoutManager &) = delete;
	TextLayoutManager(TextLayoutManager &&) = delete;
	TextLayoutManager &operator=(TextLayoutManager &&) = delete;

	virtual TextMeasurement measure(const AttributedStringBox &attributedStringBox, const ParagraphAttributes &paragraphAttributes, const TextLayoutContext &layoutContext, const LayoutConstraints &layoutConstraints) const;

	LinesMeasurements measureLines(const AttributedStringBox &attributedStringBox, const ParagraphAttributes &paragraphAttributes, const Size &size) const;

protected:
	std::shared_ptr<const ContextContainer> contextContainer_;
	SimpleThreadSafeCache<RNTextMeasureCacheKey, TextMeasurement, kSimpleThreadSafeCacheSizeCap> textMeasureCache_;

private:
	std::shared_ptr<const RNTextShaper> shaper_;
};

} // namespace facebook::react

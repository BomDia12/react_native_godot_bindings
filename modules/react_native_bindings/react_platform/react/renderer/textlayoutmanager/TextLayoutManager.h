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
	TextMeasureCache textMeasureCache_;

private:
	std::shared_ptr<const RNTextShaper> shaper_;
};

} // namespace facebook::react

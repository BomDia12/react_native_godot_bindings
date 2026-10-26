#pragma once

#include "rn_clip.h"
#include "rn_style_box.h"

// Box shadows as two canvas items under a control's own: outset shadows behind
// it, cut out where the border box sits, and inset shadows inside the padding
// box, under its children.
class RNBoxShadows {
	static constexpr int MAX_SHADOWS = 8;

	struct Layer {
		RID item;
		Ref<ShaderMaterial> material;
	};
	Layer outset;
	Layer inset;
	void _update(Layer &r_layer, RID p_parent, bool p_inset, const RNStyleBox::Paint &p_paint);
	void _free(Layer &r_layer);

public:
	~RNBoxShadows();
	void update(RID p_parent, const RNStyleBox::Paint &p_paint);
	void update_clip(const Vector<RNClip> &p_stack);
	bool is_empty() const { return outset.item.is_null() && inset.item.is_null(); }
};

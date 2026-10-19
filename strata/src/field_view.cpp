#include "field_view.h"

#include "strata/sim/colormap.hpp"

namespace mbstrata {

float saturateConcentration(float v, float half) { return v <= 0.0f ? 0.0f : v / (v + half); }

void fieldRgb(FieldView view, float tn, float tw, float to, float rgb[3]) {
    switch (view) {
        case FieldView::Nutrient: strata::sim::colormapRgb(strata::sim::Colormap::Viridis, tn, rgb); break;
        case FieldView::Waste: strata::sim::colormapRgb(strata::sim::Colormap::Magma, tw, rgb); break;
        case FieldView::Oxygen: strata::sim::colormapRgb(strata::sim::Colormap::Ice, to, rgb); break;
        case FieldView::Composite:
        default:
            rgb[0] = 0.015f + 0.030f * to + 0.090f * tn + 0.420f * tw;
            rgb[1] = 0.020f + 0.120f * to + 0.520f * tn + 0.060f * tw;
            rgb[2] = 0.040f + 0.380f * to + 0.060f * tn + 0.300f * tw;
            break;
    }
}

const char* fieldViewName(FieldView view) {
    switch (view) {
        case FieldView::Nutrient: return "nutrient";
        case FieldView::Waste: return "waste";
        case FieldView::Oxygen: return "oxygen";
        case FieldView::Composite:
        default: return "composite";
    }
}

}  // namespace mbstrata

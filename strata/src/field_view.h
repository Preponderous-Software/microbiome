#ifndef MicrobiomeStrataFieldView_h
#define MicrobiomeStrataFieldView_h

/**
 * How the micro-environment's fields are coloured: shared by the Strata scene
 * (scene.*) and the replay recorder (replay.*). No GL; links only strata::sim.
 * @author Daniel McCoy Stephenson
 */
namespace mbstrata {

enum class FieldView { Composite = 0, Nutrient = 1, Waste = 2, Oxygen = 3 };
const char* fieldViewName(FieldView view);

// How the heatmap maps a concentration onto [0, 1]. Nutrient and waste
// saturate: t = v / (v + half), so t = 0.5 at `half`. Oxygen is linear up to
// Config::oxygenSurface.
constexpr float kNutrientHalf = 4.0f;
constexpr float kWasteHalf = 3.0f;
// Rock grains are drawn raised, in this colour (linear RGB).
constexpr float kGrainRgb[3] = {0.30f, 0.25f, 0.19f};
float saturateConcentration(float v, float half);

// The colour of one open (non-grain) sample. tn, tw and to are the mapped
// nutrient, waste and oxygen levels in [0, 1]; single-field views use only
// their own one.
void fieldRgb(FieldView view, float tn, float tw, float to, float rgb[3]);

}  // namespace mbstrata

#endif

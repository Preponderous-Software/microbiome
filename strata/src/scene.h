#ifndef MicrobiomeStrataScene_h
#define MicrobiomeStrataScene_h

#include <vector>

#include "strata/strata.h"

#include "field_view.h"
#include "micro_environment.h"

/**
 * Turns a MicroEnvironment into what Strata draws: one terrain tile whose
 * per-sample colours are the field heatmap (grains raised as rock), and one
 * instanced primitive per microorganism. Rendering only; it never changes
 * the simulation.
 * @author Daniel McCoy Stephenson
 */
namespace mbstrata {

struct OrbitCamera {
    float targetX = 0, targetZ = 0;  // world point looked at (on the floor)
    float distance = 150;
    float yawDegrees = 0;            // 0: looking from +Z towards -Z
    float pitchDegrees = 64;         // above the horizon
    StrataCamera toStrata(float aspect) const;
    void pan(float dx, float dz);    // in the camera's own right/forward axes
    void zoom(float factor);
};

// A camera that frames the whole chamber.
OrbitCamera fitCamera(const MicroEnvironment& env, float aspect);

class Scene {
    public:
        // Heights (rock grains), colours from `view`, and the tile struct that
        // points at them. The tile stays valid until the next call.
        const StrataTerrainTile& buildTerrain(const MicroEnvironment& env, FieldView view);
        // One item per microorganism.
        const std::vector<StrataRenderItem>& buildItems(const MicroEnvironment& env);
        // A soft point light over each nutrient seep (at most STRATA_MAX_POINT_LIGHTS).
        const std::vector<StrataPointLight>& buildSeepLights(const MicroEnvironment& env);
    private:
        std::vector<float> heights_;
        std::vector<float> colors_;
        std::vector<StrataRenderItem> items_;
        std::vector<StrataPointLight> lights_;
        StrataTerrainTile tile_{};
};

}  // namespace mbstrata

#endif

#include "header/usdRecording.h"

UsdRecording::UsdRecording() : recorder(TICKS_PER_SECOND) {
}

void UsdRecording::recordTick(Microbiome& microbiome, size_t tick) {
    Grid* grid = microbiome.getGrid();
    recorder.begin_tick(static_cast<double>(tick));
    for (Microorganism* microorganism : microbiome.getMicroorganisms()) {
        if (microorganism->isDead() || !microbiome.isEntityPresent(*microorganism)) {
            continue;
        }
        Location& location = grid->getLocation(microorganism->getLocationId());
        recorder.entity(microorganism->getId())
            .position(location.getX(), location.getY(), 0)
            .set("kind", "microorganism")
            .set("energy", static_cast<int32_t>(microorganism->getEnergy()))
            .set("metabolicRate", static_cast<int32_t>(microorganism->getMetabolicRate()))
            .set("timesEaten", static_cast<int32_t>(microorganism->getTimesEaten()));
    }
    for (Biomatter* biomatter : microbiome.getBiomatter()) {
        if (!microbiome.isEntityPresent(*biomatter)) {
            continue;
        }
        Location& location = grid->getLocation(biomatter->getLocationId());
        recorder.entity(biomatter->getId())
            .position(location.getX(), location.getY(), 0)
            .set("kind", "biomatter")
            .set("energy", static_cast<int32_t>(biomatter->getEnergy()));
    }
    recorder.extra("alive", static_cast<int32_t>(microbiome.getNumAliveMicroorganisms()));
    recorder.extra("dead", static_cast<int32_t>(microbiome.getNumDeadMicroorganisms()));
    recorder.extra("totalEnergy", static_cast<int32_t>(microbiome.getTotalEnergy()));
    recorder.end_tick();
    ticksRecorded++;
}

size_t UsdRecording::getTicksRecorded() {
    return ticksRecorded;
}

std::string UsdRecording::toUsda() {
    return recorder.to_usda();
}

void UsdRecording::write(const std::string& path) {
    recorder.write(path);
}

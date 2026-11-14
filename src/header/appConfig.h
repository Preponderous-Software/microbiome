#ifndef App_Config_h
#define App_Config_h

#include <cstddef>

/**
 * The AppConfig class contains the configuration for the application and 
 * is used in the MicrobiomeApplication and Simulation classes.
 * @author Daniel McCoy Stephenson
 */
class AppConfig {
    public:
        size_t getEnvironmentSize();
        size_t getEntityFactor();
        size_t getMaxTicks();
        size_t getTickLengthInSeconds();
        size_t getNumSimulations();
        bool isSimulationOutputEnabled();
        // The branching-colony scenario (BranchingColony): off by default, in
        // which case the app runs its usual Microbiome simulations.
        bool isColonyScenarioEnabled();
        bool isColonyNutrientRich();
        size_t getColonyGridSize();

        void setEnvironmentSize(int environmentSize);
        void setEntityFactor(int entityFactor);
        void setMaxTicks(int maxTicks);
        void setTickLengthInSeconds(int tickLengthInSeconds);
        void setNumSimulations(int numSimulations);
        void setSimulationOutputEnabled(bool simulationOutputEnabled);
        void setColonyScenarioEnabled(bool colonyScenarioEnabled);
        void setColonyNutrientRich(bool colonyNutrientRich);
        void setColonyGridSize(int colonyGridSize);
    private:
        size_t environmentSize = 10;
        size_t entityFactor = 10;
        size_t maxTicks = 0; // set to 0 to run indefinitely until all microorganisms die
        size_t tickLengthInSeconds = 0; // set to 0 to run as fast as possible
        size_t numSimulations = 10;
        bool simulationOutputEnabled = true;
        bool colonyScenarioEnabled = false;
        bool colonyNutrientRich = false; // false: nutrient-poor, branching growth
        size_t colonyGridSize = 201;
};

#endif
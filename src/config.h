#ifndef Config_h
#define Config_h

class Config {
    public:
        int getEnvironmentSize();
        int getEntityFactor();
        int getMaxTicks();
        int getTickLengthInSeconds();
    private:
        int environmentSize = 10;
        int entityFactor = 3;
        int maxTicks = 100;
        int tickLengthInSeconds = 1;
};

#endif
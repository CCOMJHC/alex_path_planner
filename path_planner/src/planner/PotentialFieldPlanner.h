#ifndef SRC_POTENTIALFIELDPLANNER_H
#define SRC_POTENTIALFIELDPLANNER_H


#include "Planner.h"

class PotentialFieldPlanner : public Planner {
public:
    ~PotentialFieldPlanner() override = default;

    Stats plan(
        const RibbonManager& ribbonManager,
        const State& start,
        PlannerConfig config,
        const DubinsPlan& previousPlan,
        double timeRemaining,
        std::unordered_map<uint32_t, GaussianDynamicObstaclesManager::Obstacle> dynamic_obstacles_copy
    ) override;

private:
    struct Force {
        Force() = default;
        Force(double magnitude, double direction) {
            X = magnitude * cos(direction);
            Y = magnitude * sin(direction);
        }
        double X, Y;
        Force operator+(const Force& other) const {
            Force result{};
            result.X = X + other.X;
            result.Y = Y + other.Y;
            return result;
        }
        Force operator-(const Force& other) const {
            Force result{};
            result.X = X - other.X;
            result.Y = Y - other.Y;
            return result;
        }

        double getDirection() const {
            // radians north of east
            return atan2(Y, X);
        }
    };

    static double getRibbonMagnitude(double distance)  {
        // avoid dividing by 0 with a max value
        if (distance <= 0.5) return 20;
        return 10 / distance;
    }

    static double getDynamicObstacleMagnitude(double distance, double width, double length) {
        // if we're super close just return a really high value
        if (distance <= 0) return 1000;
        // scale magnitude by obstacle area
        return exp(-distance / 13) * width * length / 10;
//        return width * length / distance / distance / 100;
    }

    static double getStaticObstacleMagnitude(double distance) {
        if (distance > c_StaticObsIgnoreThreshold) return 0;
        return exp(-distance / 15);
    }

    static constexpr int c_LookaheadSteps = 10;

    static constexpr double c_StaticObsIgnoreThreshold = 7.5;
};


#endif //SRC_POTENTIALFIELDPLANNER_H

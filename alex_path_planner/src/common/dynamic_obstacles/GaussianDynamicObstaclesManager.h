#ifndef SRC_GAUSSIANDYNAMICOBSTACLESMANAGER_H
#define SRC_GAUSSIANDYNAMICOBSTACLESMANAGER_H

#include "DynamicObstaclesManagerBase.h"
#include <eigen3/Eigen/Core>
#include <eigen3/Eigen/LU>
#include <utility>

/**
 * Class to manage Gaussian-like dynamic obstacles.
 *
 * This class depends on Eigen, a header library that's available with "sudo apt install libeigen3-dev"
 */
class GaussianDynamicObstaclesManager : public DynamicObstaclesManagerBase {
public:
    typedef std::shared_ptr<GaussianDynamicObstaclesManager> SharedPtr;

    struct Obstacle {
        double X, Y, Yaw, Speed, Time;
        Eigen::Vector2d mean;
        Eigen::Matrix<double, 2, 2> covariance;
        Obstacle(double x, double y, double heading, double speed, double time)
                : X(x), Y(y), Yaw(M_PI_2 - heading), Speed(speed), Time(time), mean(x, y){
            covariance << 30, 10,
                          10, 30;
        }

        Obstacle(double x, double y, double heading, double speed, double time, Eigen::Matrix<double, 2, 2> covariance)
                : X(x), Y(y), Yaw(M_PI_2 - heading), Speed(speed), Time(time), mean(x, y), covariance(std::move(covariance)) {}

        Obstacle(const Obstacle & other) :
            X(other.X),
            Y(other.Y),
            Yaw(other.Yaw),
            Speed(other.Speed),
            Time(other.Time),
            mean(other.mean),
            covariance(other.covariance)
        {}

        void project(double desiredTime) {
            double dt = desiredTime - Time;
            double dx = Speed * dt * cos(Yaw);
            double dy = Speed * dt * sin(Yaw);
            X += dx; Y += dy;
            mean = Eigen::Vector2d(X, Y);
        }

        double pdf(const Eigen::Vector2d& x) const {
            double twoPi = 2 * M_PI;
            double quadform  = (x - mean).transpose() * covariance.inverse() * (x - mean);
            double norm = 1.0 / twoPi / std::sqrt(covariance.determinant());
            return norm * exp(-0.5 * quadform);
        }
    };

    ~GaussianDynamicObstaclesManager() override = default;

    double collisionExists(double x, double y, double time, bool strict) const override;

    void update(uint32_t mmsi, double x, double y, double heading, double speed, double time);

    void update(uint32_t mmsi, double x, double y, double heading, double speed, double time, Eigen::Matrix<double, 2, 2> covariance);

    void forget(uint32_t mmsi);

    const std::unordered_map<uint32_t, Obstacle>& get() const;

    const std::unordered_map<uint32_t, Obstacle> get_deep_copy() const;

    int size() const;

private:
    std::unordered_map<uint32_t, Obstacle> m_Obstacles;
};


#endif //SRC_GAUSSIANDYNAMICOBSTACLESMANAGER_H

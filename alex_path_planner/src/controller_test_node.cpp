#include <future>
#include <vector>
#include "ros/ros.h"
#include "geometry_msgs/TwistStamped.h"
#include "project11_msgs/NavEulerStamped.h"
#include "alex_path_planner/alex_path_plannerAction.h"
#include <geometry_msgs/PoseStamped.h>
#include "trajectory_publisher.h"
#include "NodeBase.h"

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCInconsistentNamingInspection"
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

/**
 * Node to test the model-predictive controller independently of the path planner. This node pretends to be the
 * path planner node
 */
class ControllerTest : public NodeBase
{
public:
    explicit ControllerTest(std::string name):
            NodeBase(name)
    {}

    void goalCallback() override
    {
        m_Preempted = false;

        auto goal = m_action_server.acceptNewGoal();

        // make sure controller is up
        publishControllerMessage("start running");

        publishControllerMessage("start sending controls");

        std::vector<std::pair<double, double>> currentPath;

        std::cerr << "Received " << goal->path.poses.size() - 1 << " survey line(s)" << std::endl;

        m_Trajectory.clear();

        double time = getTime() /*+ 10*/; // give the controller an extra 10s to get to the line

        DubinsPlan plan;
        for (unsigned long i = 0; i < goal->path.poses.size() - 1; i ++) {
            auto startPoint = m_CoordinateConverter.wgs84_to_map(goal->path.poses[i].pose.position);
            auto endPoint = m_CoordinateConverter.wgs84_to_map(goal->path.poses[i + 1].pose.position);
            State start(startPoint.x, startPoint.y, 0, c_MaxSpeed, time);
            State end(endPoint.x, endPoint.y, 0, 0, 0);
            start.setHeadingTowards(end);
            end.setHeading(start.heading());
            DubinsWrapper wrapper(start, end, 8); // why 8? I just picked it OK? it doesn't actually matter
            std::cerr << "Adding line between\n" << start.toString() << " and\n" << end.toString() << std::endl;
            plan.append(wrapper);
            // still append to trajectory so we can display the start state
            State current = start;
            auto d = start.distanceTo(end);
            for (int j = 0; j < d; j++){ // 2 m/s updated every half second is 1m of distance each iteration
                current = start.push(0.5 * j);
                m_Trajectory.push_back(current);
            }
            m_Trajectory.push_back(current);
            time += wrapper.length() / c_MaxSpeed;
        }

        m_Plan = plan;

        std::cerr << "Publishing a plan of length " << plan.get().size() << " to controller" << std::endl;

        m_TrajectoryDisplayer.displayTrajectory(plan.getHalfSecondSamples(), true);
//        publishTrajectory(m_Trajectory);

        // not sure what's needed for this test; just doing here what I do for RBPC in Executive
        double planning_time_ideal = 1.0;
        publishPlan(plan, planning_time_ideal);

        auto t = async(std::launch::async, [&]{
//            cerr << "Starting display loop at time " << t << endl;
            int i = 0;
//            sleep(11); // sleep a little extra to make sure we've started the plan
            State sample;
            sample.time() = getTime();
            while (plan.containsTime(sample.time())) {
                plan.sample(sample);
                displayPlannerStart(sample);
                sleep(1);
                sample.time() = getTime();
                if (m_Preempted) {
                    break;
                }
            }
            if (m_Preempted) {
                m_Preempted = false;
            } else {
                m_ActionDone = true;
            }
            clearDisplay();
        });
    }

    void preemptCallback() override
    {
        std::cerr << "Canceling controller test run" << std::endl;
        m_action_server.setPreempted();
        m_Preempted = true;

        // Should the executive stop now? Probably?
        publishControllerMessage("stop sending controls");
        clearDisplay();
    }

    void odometryCallback(const nav_msgs::Odometry::ConstPtr &inmsg) override
    {
        if (m_ActionDone) allDone();
    }

//    void publishTrajectory(std::vector<State> trajectory)
//    {
//        alex_path_planner::Trajectory reference;
//        for (State s : trajectory) {
//            reference.states.push_back(convertToStateMsg(s));
//        }
//        reference.trajectoryNumber = ++m_TrajectoryCount;
//        mpc::UpdateReferenceTrajectoryRequest req;
//        mpc::UpdateReferenceTrajectoryResponse res;
//        req.trajectory = reference;
//        if (m_update_reference_trajectory_client.call(req, res)) {
//            // success
//        } else {
//            std::cerr << "Controller failed to send state to test node" << std::endl;
//        }
//    }

    void allDone() override
    {
        m_ActionDone = false;
        alex_path_planner::alex_path_plannerResult result;
        m_action_server.setSucceeded(result);
        std::cerr << "The times in the trajectory have now all passed. Setting the succeeded bit in the action server." << std::endl;

        publishControllerMessage("stop sending controls");
    }

    void displayDot(const State& s) {
        std::cerr << "Displaying dot at state " << s.toString() << std::endl;
        geographic_visualization_msgs::GeoVizItem geoVizItem;
        geoVizItem.id = "reference_tracker";
        geographic_visualization_msgs::GeoVizPointList displayPoints;
        displayPoints.color.r = 1;
        displayPoints.color.g = 1;
        displayPoints.color.b = 1;
        displayPoints.color.a = 0.5;
        displayPoints.size = 8;
        displayPoints.points.push_back(convertToLatLong(s));
        geoVizItem.lines.push_back(displayPoints);
        m_display_pub.publish(geoVizItem);
    }

    void pilotingModeCallback(const std_msgs::String::ConstPtr& inmsg) override {
        // ???
    }

private:
    std::vector<State> m_Trajectory;
    DubinsPlan m_Plan;

    static constexpr double c_MaxSpeed = 2.0;
};

int main(int argc, char **argv)
{
    std::cerr << "Starting controller test node" << std::endl;
    ros::init(argc, argv, "controller_test");
    ControllerTest pp("path_planner_action");
    ros::spin();

    return 0;
}

#pragma clang diagnostic pop

#ifndef SRC_NODEBASE_H
#define SRC_NODEBASE_H

#include "ros/ros.h"
#include "std_msgs/String.h"
#include <vector>
#include "alex_path_planner/alex_path_plannerAction.h"
#include "actionlib/server/simple_action_server.h"
#include <geometry_msgs/PoseStamped.h>
#include "trajectory_publisher.h"
#include "alex_path_planner_common/TrajectoryDisplayerHelper.h"
#include <alex_path_planner_common/UpdateReferenceTrajectory.h>
#include <alex_path_planner_common/DubinsPlan.h>
#include <alex_path_planner_common/DubinsPath.h>
#include <geographic_visualization_msgs/GeoVizItem.h>
#include <project11/tf2_utils.h>
#include <nav_msgs/Odometry.h>

#include <iostream>

/**
 * Base class for nodes related to the path planner. Holds some shared code and does some shared setup.
 */
class NodeBase
{
public:
    explicit NodeBase(std::string name):
            m_action_server(m_node_handle, std::move(name), false)
    {
        m_controller_msgs_pub = m_node_handle.advertise<std_msgs::String>("controller_msgs",1);
        m_display_pub = m_node_handle.advertise<geographic_visualization_msgs::GeoVizItem>("project11/display",1);

        m_update_reference_trajectory_client = m_node_handle.serviceClient<alex_path_planner_common::UpdateReferenceTrajectory>("mpc/update_reference_trajectory");

        m_piloting_mode_sub = m_node_handle.subscribe("project11/piloting_mode", 10, &NodeBase::pilotingModeCallback, this);
        m_odom_sub = m_node_handle.subscribe("odom", 10, &NodeBase::odometryCallback, this);

        m_action_server.registerGoalCallback(boost::bind(&NodeBase::goalCallback, this));
        m_action_server.registerPreemptCallback(boost::bind(&NodeBase::preemptCallback, this));
        m_action_server.start();

        
        ros::NodeHandle nh_private("~");
        nh_private.param<std::string>("map_frame", m_map_frame, "map");

        m_TrajectoryDisplayer = TrajectoryDisplayerHelper(m_node_handle, &m_display_pub, m_CoordinateConverter, m_map_frame);
        

    }

    ~NodeBase()
    {
        publishControllerMessage("terminate");
    }

    /**
     * Goal callback for action server.
     */
    virtual void goalCallback() = 0;

    /**
     * Preempt callback for action server.
     */
    virtual void preemptCallback() = 0;

    virtual void odometryCallback(const nav_msgs::Odometry::ConstPtr &inmsg)
    {
      m_odometry = inmsg;
    }

    /**
     * Callback to update piloting mode.
     * @param inmsg
     */
    virtual void pilotingModeCallback(const std_msgs::String::ConstPtr& inmsg) = 0;

    /**
     * What to do when the planner finishes.
     */
    virtual void allDone() = 0;

    /**
     * Publish a message to the controller.
     * @param m
     */
    void publishControllerMessage(std::string m)
    {
        std_msgs::String msg;
        msg.data = std::move(m);
        m_controller_msgs_pub.publish(msg);
    }

    /**
     * Display the contents of a ribbon manager.
     * @param ribbonManager
     */
    void displayRibbons(const RibbonManager& ribbonManager) {

        geographic_visualization_msgs::GeoVizItem geoVizItem;

        for (const auto& r : ribbonManager.get()) {
            geographic_visualization_msgs::GeoVizPointList displayPoints;
            displayPoints.color.r = 1;
            displayPoints.color.b = 0.5;
            displayPoints.color.a = 0.6;
            displayPoints.size = 15;
            geographic_msgs::GeoPoint point;
            displayPoints.points.push_back(convertToLatLong(r.startAsState()));
            displayPoints.points.push_back(convertToLatLong(r.endAsState()));
            geoVizItem.lines.push_back(displayPoints);
        }
        geoVizItem.id = "ribbons";
        m_display_pub.publish(geoVizItem);
    }

    /**
     * Display the start state for the current planning iteration.
     * @param state
     */
    void displayPlannerStart(const State& state) {
//        cerr << "Displaying state " << state.toString() << endl;
        geographic_visualization_msgs::GeoVizItem geoVizItem;
        geographic_visualization_msgs::GeoVizPolygon polygon;
        geographic_visualization_msgs::GeoVizSimplePolygon simplePolygon;
        State bow, sternPort, sternStarboard;
        bow = state.push(3 / state.speed()); //set bow 3m ahead of state
        sternPort = state.push( -1 / state.speed());
        sternStarboard = sternPort;
        auto a = state.heading() + M_PI_2;
        auto dx = 1.5 * sin(a);
        auto dy = 1.5 * cos(a);
        sternPort.x() += dx;
        sternPort.y() += dy;
        sternStarboard.x() -= dx;
        sternStarboard.y() -= dy;
        simplePolygon.points.push_back(convertToLatLong(bow));
        simplePolygon.points.push_back(convertToLatLong(sternPort));
        simplePolygon.points.push_back(convertToLatLong(sternStarboard));
        polygon.outer = simplePolygon;
        polygon.edge_color.b = 1;
        polygon.edge_color.a = 0.7;
        polygon.fill_color = polygon.edge_color;
        geoVizItem.polygons.push_back(polygon);
        geoVizItem.id = "planner_start";
        m_display_pub.publish(geoVizItem);
    }

    /**
     * Clear the display. Not sure why this doesn't work.
     */
    void clearDisplay() {
        displayRibbons(RibbonManager());
        m_TrajectoryDisplayer.displayTrajectory(std::vector<State>(), true);
        geographic_visualization_msgs::GeoVizItem geoVizItem;
        geoVizItem.id = "planner_start";
        m_display_pub.publish(geoVizItem);
        geoVizItem.id = "reference_tracker";
        m_display_pub.publish(geoVizItem);
    }

    /**
     * Utility to get the time.
     * @return
     */
    double getTime() const {
        return m_TrajectoryDisplayer.getTime();
    }

    /**
     * Convert a state (local map coordinates) to a GeoPoint (LatLong).
     * @param state
     * @return
     */
    geographic_msgs::GeoPoint convertToLatLong(const State& state) {
        return m_TrajectoryDisplayer.convertToLatLong(state);
    }

    /**
     * Convert an internal Dubins plan to a ROS message.
     * @param plan
     * @return
     */
    static alex_path_planner_common::Plan convertToPlanMsg(const DubinsPlan& plan) {
        alex_path_planner_common::Plan planMsg;
        for (const auto& d : plan.get()) {
            alex_path_planner_common::DubinsPath path;
            auto p = d.unwrap();
            path.initial_x = p.qi[0];
            path.initial_y = p.qi[1];
            path.initial_yaw = p.qi[2];
            path.length0 = p.param[0];
            path.length1 = p.param[1];
            path.length2 = p.param[2];
            path.type = p.type;
            path.rho = d.getRho();
            path.speed = d.getSpeed();
            path.start_time = d.getStartTime();
            planMsg.paths.push_back(path);
        }
        planMsg.endtime = plan.getEndTime();
        // std::cerr << "DEBUG: NodeBase.convertToPlanMsg about to return this planMsg:\n" << std::endl;
        // std::cerr << "DEBUG:    number of paths: " << planMsg.paths.size() << " ... end time: " << planMsg.endtime << std::endl;
        for (const auto& p : planMsg.paths) {
            // std::cerr << "DEBUG: a path starts at time t = " << p.start_time << std::endl;
        }
        // std::cerr << "DEBUG: start time: " << planMsg.start_time << std::endl;
        // std::cerr << "DEBUG: end time: " << planMsg.endtime << std::endl;
        return planMsg;
    }

    /**
     * Update the controller's reference trajectory and return the state it provides.
     * @param plan
     * @return
     */
    State publishPlan(const DubinsPlan& plan, double planning_time_ideal) {
        // std::cerr << "NodeBase.publishPlan called with plan: " << plan.getStartTime() << " to " << plan.getEndTime() << std::endl;
        alex_path_planner_common::UpdateReferenceTrajectoryRequest req;
        alex_path_planner_common::UpdateReferenceTrajectoryResponse res;
        req.plan = convertToPlanMsg(plan);
        req.planning_time = planning_time_ideal;
        if (m_update_reference_trajectory_client.call(req, res)) {
            auto s = m_TrajectoryDisplayer.convertToStateFromMsg(res.state);
            displayPlannerStart(s);
            return s;
        } else {
            // DEBUG
            std::cerr << "NodeBase.publishPlan: m_update_refrence_trajectory_client.call() failed; returning new default State()" << std::endl;
            return State();
        }
    }

protected:
    ros::NodeHandle m_node_handle;

    TrajectoryDisplayerHelper m_TrajectoryDisplayer;

    actionlib::SimpleActionServer<alex_path_planner::alex_path_plannerAction> m_action_server;

    // Apparently the action server is supposed to be manipulated on the ROS thread, so I had to make some flags
    // to get the done/preemption behavior I wanted. It seems like there should be a better way to do this but I don't
    // know what it is. We start in the done state because we haven't received a goal yet.
    bool m_ActionDone = false, m_Preempted = true;

    ros::Publisher m_controller_msgs_pub;
    ros::Publisher m_display_pub;

    ros::Subscriber m_odom_sub;
    ros::Subscriber m_piloting_mode_sub;
    nav_msgs::Odometry::ConstPtr m_odometry;


    ros::ServiceClient m_update_reference_trajectory_client;

    project11::Transformations m_CoordinateConverter;
    std::string m_map_frame;
};


#endif //SRC_NODEBASE_H

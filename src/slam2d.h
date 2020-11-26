#ifndef __SLAM2D_H
#define __SLAM2D_H

#include <iostream>
#include <Eigen/Eigen>
#include <sensor_msgs/MultiEchoLaserScan.h>
#include <sensor_msgs/LaserScan.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/kdtree/kdtree_flann.h>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/visualization/cloud_viewer.h>

using namespace std;
using namespace Eigen;


typedef pcl::PointXY PointType;

typedef struct
{
    float x;
    float y;
    float theta;
} state2d;

Eigen::Vector2d point2eigen(PointType p)
{
    Eigen::Vector2d pp;
    pp(0) = p.x;
    pp(1) = p.y;
    return pp;
}

PointType eigen2point(Eigen::Vector2d pp)
{
    PointType p;
    p.x = pp(0);
    p.y = pp(1);
    return p;
}

class slam2d
{
private:
    /* data */
public:
    slam2d(/* args */);
    ~slam2d();
    state2d state;
    state2d delta;
    pcl::PointCloud<PointType> scan;
    pcl::PointCloud<PointType> scan_prev;

    pcl::PointCloud<PointType> scan_normal;

    void readin_scan_data(const sensor_msgs::MultiEchoLaserScanConstPtr &msg);
    void readin_scan_data(const sensor_msgs::LaserScanConstPtr &msg);

    void update_scan_normal();
    void scan_match();
    void update(const sensor_msgs::MultiEchoLaserScanConstPtr& msg);
    void update(const sensor_msgs::LaserScanConstPtr &msg);
};

slam2d::slam2d(/* args */)
{
}

slam2d::~slam2d()
{
}

void slam2d::readin_scan_data(const sensor_msgs::MultiEchoLaserScanConstPtr &msg)
{
    scan.points.resize(msg->ranges.size());
    for (auto i = 0; i < msg->ranges.size(); i++)
    {
        //scan.points[i]
        float dist = msg->ranges[i].echoes[0]; //only first echo used for slam2d
        float theta = msg->angle_min + i * msg->angle_increment;
        scan.points[i].x = dist * cos(theta);
        scan.points[i].y = dist * sin(theta);
        //printf("%f,", scan[i]);
    }
    scan.width = scan.points.size();
    scan.height = 1;
    scan.is_dense = true;
}
void slam2d::readin_scan_data(const sensor_msgs::LaserScanConstPtr &msg)
{

}

void slam2d::update_scan_normal()
{
    //compute normal of scan
    scan_normal.points.resize(scan.points.size());
    for (auto i = 1; i < scan.points.size(); i++)
    {
        PointType p1 = scan.points[i - 1];
        PointType p2 = scan.points[i];

        Eigen::Vector2d delta(p1.x - p2.x, p1.y - p2.y);
        Eigen::Vector2d normal(-delta(1), delta(0));
        normal /= normal.norm();
        scan_normal.points[i].x = normal(0);
        scan_normal.points[i].y = normal(1);
    }
}
void slam2d::scan_match()
{
    state2d delta;
    delta.x = 0;
    delta.y = 0;
    delta.theta = 0;

    if (scan.points.size() && scan_prev.points.size())
    {
        //solve delta with ceres constraints
        pcl::KdTreeFLANN<PointType> kdtree;
        kdtree.setInputCloud(scan.makeShared());
        int K = 2; // K nearest neighbor search
        std::vector<int> index(K);
        std::vector<float> distance(K);
        //1. project scan_prev to scan

        Eigen::Matrix2d R;
        R(0, 0) = cos(delta.theta); R(0, 1) = -sin(delta.theta);
        R(1, 0) = sin(delta.theta); R(1, 1) = cos(delta.theta);
        Eigen::Vector2d dt(delta.x, delta.y);
        //find nearest neighur
        for (int i = 0; i < scan_prev.points.size(); i++)
        {
            PointType search_point = scan_prev.points[i];
            //project search_point to current frame
            PointType search_point_predict = eigen2point(R * point2eigen(search_point) + dt);
            if (kdtree.nearestKSearch(search_point_predict, K, index, distance) == K)
            {
                //add constraints
                Eigen::Vector3d p = point2eigen(search_point);
                Eigen::Vector3d p1 = point2eigen(scan.points[index[0]]);
                Eigen::Vector3d p2 = point2eigen(scan.points[index[1]]);
                ceres::CostFunction *cost_function = lidar_edge_error::Create(p, p1, p2);
                problem.AddResidualBlock(cost_function,
                                         new CauchyLoss(0.5),
                                         pose);
            }
        }
    }
}
void slam2d::update(const sensor_msgs::MultiEchoLaserScanConstPtr &msg)
{
    readin_scan_data(msg);
}

void slam2d::update(const sensor_msgs::LaserScanConstPtr &msg)
{
    readin_scan_data(msg);
}
#endif
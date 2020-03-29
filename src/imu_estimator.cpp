/*
 * imu_ekf 
 *
 * Copyright 2016-2017 Stylianos Piperakis, Foundation for Research and Technology Hellas (FORTH)
 * License: BSD
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University of Freiburg nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <iostream>
#include <algorithm>
#include "imu_ekf/imu_estimator.h"


void imu_estimator::loadparams() {

	ros::NodeHandle n_p("~");

	n_p.getParam("imu_topic_freq",freq);
	n_p.getParam("useVO",useVO);
	n_p.getParam("useOdom",useOdom);
	n_p.getParam("useTwist",useTwist);
	
	n_p.getParam("camera_vo_topic", camera_vo_topic);
	n_p.getParam("odom_topic", odom_topic);
	n_p.getParam("imu_topic", imu_topic);
	n_p.getParam("twist_topic",twist_topic);
}



void imu_estimator::loadIMUEKFparams()
{
	ros::NodeHandle n_p("~");
	n_p.getParam("biasAX", biasAX);
	n_p.getParam("biasAY", biasAY);
	n_p.getParam("biasAZ", biasAZ);
	n_p.getParam("biasGX", biasGX);
	n_p.getParam("biasGY", biasGY);
	n_p.getParam("biasGZ", biasGZ);

	n_p.getParam("AccSTDx", imuEKF->AccSTDx);
	n_p.getParam("AccSTDy", imuEKF->AccSTDy);
	n_p.getParam("AccSTDz", imuEKF->AccSTDz);

	n_p.getParam("GyroSTDx", imuEKF->GyroSTDx);
	n_p.getParam("GyroSTDy", imuEKF->GyroSTDy);
	n_p.getParam("GyroSTDz", imuEKF->GyroSTDz);

	n_p.getParam("AccBiasSTDx", imuEKF->AccBiasSTDx);
	n_p.getParam("AccBiasSTDy", imuEKF->AccBiasSTDy);
	n_p.getParam("AccBiasSTDz", imuEKF->AccBiasSTDz);
	n_p.getParam("GyroBiasSTDx", imuEKF->GyroBiasSTDx);
	n_p.getParam("GyroBiasSTDy", imuEKF->GyroBiasSTDy);
	n_p.getParam("GyroBiasSTDz", imuEKF->GyroBiasSTDz);

	n_p.getParam("KinSTDx", imuEKF->KinSTDx);
	n_p.getParam("KinSTDy", imuEKF->KinSTDy);
	n_p.getParam("KinSTDz", imuEKF->KinSTDz);
	
	n_p.getParam("KinSTDOrientx", imuEKF->KinSTDOrientx);
	n_p.getParam("KinSTDOrienty", imuEKF->KinSTDOrienty);
	n_p.getParam("KinSTDOrientz", imuEKF->KinSTDOrientz);
	n_p.getParam("gravity", imuEKF->ghat);

}





void imu_estimator::subscribeToIMU()
{
	imu_sub = n.subscribe(imu_topic,1,&imu_estimator::imuCb,this);
}

void imu_estimator::imuCb(const sensor_msgs::Imu::ConstPtr& msg)
{
	imu_msg = *msg;
	imu_inc = true;
}

void imu_estimator::subscribeToOdom()
{
	odom_sub = n.subscribe(odom_topic,1,&imu_estimator::odomCb,this);
}

void imu_estimator::odomCb(const nav_msgs::Odometry::ConstPtr& msg)
{
	std::cout<<"2 "<<std::endl;//这里一直进来
	odom_msg = *msg;
	odom_inc = true;
}


void imu_estimator::subscribeToVO()
{
	vo_sub = n.subscribe(camera_vo_topic,1000,&imu_estimator::voCb,this);
}

void imu_estimator::voCb(const geometry_msgs::PoseStamped::ConstPtr& msg)
{
	std::cout<<"1 "<<std::endl;//这里进不来
	vo_msg = *msg;
	vo_inc = true;
}

void imu_estimator::subscribeToTwist()
{
	twist_sub = n.subscribe(twist_topic,1,&imu_estimator::twistCb,this);
}

void imu_estimator::twistCb(const geometry_msgs::TwistStamped::ConstPtr& msg)
{
	twist_msg = *msg;
	twist_inc = true;
}

imu_estimator::imu_estimator()  //imu_estimator这个构造函数刚开始的时候是什么都不用的
{
	useVO = false;
	useOdom = false;
	useTwist = false;
}

imu_estimator::~imu_estimator() 
{
	if (is_connected_)
		disconnect();
}


void imu_estimator::disconnect() {
	if (!is_connected_)
		return;
	
	is_connected_ = false;
}

bool imu_estimator::connect(const ros::NodeHandle nh) {
	// Initialize ROS nodes
	n = nh;
	// Load ROS Parameters
	loadparams();
	//Initialization
	init();
	// Load IMU parameters
	loadIMUEKFparams();

	//后面这段重新从paramcontrol.cfg中读了一遍imu参数，把前面的都覆盖读了一遍
	/*dynamic_recfg_ = boost::make_shared< dynamic_reconfigure::Server<imu_ekf::ParamControlConfig> >(n);
    dynamic_reconfigure::Server<imu_ekf::ParamControlConfig>::CallbackType cb = boost::bind(&imu_estimator::reconfigureCB, this, _1, _2);
    dynamic_recfg_->setCallback(cb);*/

	// Subscribe/Publish ROS Topics/Services
	subscribe();
	advertise();

	is_connected_ = true;

	ROS_INFO_STREAM("IMU Estimator Initialized");

	return true;
}


bool imu_estimator::connected() {
	return is_connected_;
}

void imu_estimator::subscribe()
{
	subscribeToIMU();

	if (useVO)
	{	
		
		subscribeToVO();
	}
	//else if(useOdom)
	if(useOdom)
	{
		
		subscribeToOdom();
	}
		
	if(useTwist)
		subscribeToTwist();
	sleep(1.0);

}

void imu_estimator::init() {
	// Initialize the IMU based EKF 
	imuEKF = new IMUEKF;
	imuEKF->init();

	imu_inc = false;
	vo_inc = false;
	odom_inc = false;
	twist_inc = false;
}

void imu_estimator::reconfigureCB(imu_ekf::ParamControlConfig& config, uint32_t level)
{
      imuEKF->x(9) = config.biasGX;
      imuEKF->x(10) = config.biasGY;
      imuEKF->x(11) = config.biasGZ;
	  imuEKF->x(12) = config.biasAX;
      imuEKF->x(13) = config.biasAY;
      imuEKF->x(14) = config.biasAZ;

      imuEKF->AccBiasSTDx = config.AccBiasSTDx;
      imuEKF->AccBiasSTDy = config.AccBiasSTDy;
      imuEKF->AccBiasSTDz = config.AccBiasSTDz;
      imuEKF->GyroBiasSTDz = config.GyroBiasSTDz;
      imuEKF->GyroBiasSTDy = config.GyroBiasSTDy;
      imuEKF->GyroBiasSTDx = config.GyroBiasSTDx;

      imuEKF->AccSTDx = config.AccSTDx;
      imuEKF->AccSTDy = config.AccSTDy;
      imuEKF->AccSTDz = config.AccSTDz;


      imuEKF->GyroSTDx = config.GyroSTDx;
      imuEKF->GyroSTDy = config.GyroSTDy;
      imuEKF->GyroSTDz = config.GyroSTDz;

      imuEKF->KinSTDx = config.KinSTDx; 
      imuEKF->KinSTDy = config.KinSTDy; 
      imuEKF->KinSTDz = config.KinSTDz; 

      imuEKF->KinSTDOrientx = config.KinSTDOrientx; 
      imuEKF->KinSTDOrienty = config.KinSTDOrienty; 
      imuEKF->KinSTDOrientz = config.KinSTDOrientz; 
}

void imu_estimator::run() {
	while (ros::ok()) {

		//是否用imu作为预测
		predictWithImu = false;
		static ros::Rate rate(freq);  //ROS Node Loop Rate

		estimateWithIMUEKF();

		//Publish Data
		publishBodyEstimates();

		ros::spinOnce();
		rate.sleep();
	}
	//De-allocation of Heap
	deAllocate();
}

void imu_estimator::estimateWithIMUEKF()
{
		//Initialize the IMU EKF state
		if (imuEKF->firstrun) {
			imuEKF->setdt(1.0/freq);
			//std::cout<<freq<<std::endl; 这个也会根据那个文件更改
			imuEKF->setBodyPos(Vector3d(0,0,0));
			imuEKF->setBodyOrientation(Matrix3d::Identity());
			imuEKF->setAccBias(Vector3d(biasAX, biasAY, biasAZ));
			imuEKF->setGyroBias(Vector3d(biasGX, biasGY, biasGZ));
			imuEKF->firstrun = false;
		}

		//Compute the attitude and posture with the IMU-Kinematics Fusion
		
		//Predict Step
		//Predict with the IMU gyro and acceleration
		if(imu_inc &&!imuEKF->firstrun){
			//std::cout<<"predictWithImu "<<std::endl;
			imuEKF->timestamps=imu_msg.header.stamp.sec+imu_msg.header.stamp.nsec*1e-9;
			imuEKF->predict(Vector3d(imu_msg.angular_velocity.x,imu_msg.angular_velocity.y,imu_msg.angular_velocity.z),
			Vector3d(imu_msg.linear_acceleration.x,imu_msg.linear_acceleration.y,imu_msg.linear_acceleration.z));
			imu_inc = false;
			predictWithImu = true;
		}

		//UPDATE STEP
		if(useOdom)
		{
			//std::cout<<"useOdom "<<std::endl; //这个是会进来的
			if(odom_inc ) // if(odom_inc && !predictWithImu)
			{
				//std::cout<<"Odom "<<std::endl; //这个是会进来的
				imuEKF->timestamps=odom_msg.header.stamp.sec+odom_msg.header.stamp.nsec*1e-9;
				imuEKF->updateWithOdom(Vector3d(odom_msg.pose.pose.position.x,odom_msg.pose.pose.position.y,odom_msg.pose.pose.position.z),
				Quaterniond(odom_msg.pose.pose.orientation.w,odom_msg.pose.pose.orientation.x,odom_msg.pose.pose.orientation.y,odom_msg.pose.pose.orientation.z));
				odom_inc = false;
				predictWithImu= false;
			}
		}
		//else if(useVO)
		if(useVO)
		{
			//1028.bag这个数据刚开始VO漂移了，数据比较差就去掉了，所以开始监听不到VO
			if(vo_inc ) // if(vo_inc && !predictWithImu)
			{
				std::cout<<"VO "<<std::endl; 
				imuEKF->timestamps=vo_msg.header.stamp.sec+vo_msg.header.stamp.nsec*1e-9;
				imuEKF->updatewithVO(Vector3d(vo_msg.pose.position.x,vo_msg.pose.position.y,vo_msg.pose.position.z),
				Quaterniond(vo_msg.pose.orientation.w,vo_msg.pose.orientation.x,vo_msg.pose.orientation.y,vo_msg.pose.orientation.z));
				vo_inc = false;
				predictWithImu= false;
			}
		}
		if(useTwist)
		{		
			if(twist_inc ) //if(odom_inc && !predictWithImu)
			{
				imuEKF->timestamps=twist_msg.header.stamp.sec+twist_msg.header.stamp.nsec*1e-9;
				imuEKF->updateWithTwist(Vector3d(twist_msg.twist.linear.x,twist_msg.twist.linear.y,twist_msg.twist.linear.z),
				Quaterniond(imu_msg.orientation.w,imu_msg.orientation.x,imu_msg.orientation.y,imu_msg.orientation.z));
				twist_inc = false;
				predictWithImu= false;
			}
		}
				
}



void imu_estimator::deAllocate()
{
	delete imuEKF;
}


void imu_estimator::publishBodyEstimates() {

	bodyPose_est_msg.header.stamp = ros::Time::now();
	bodyPose_est_msg.header.frame_id = "odom";
	bodyPose_est_msg.pose.position.x = imuEKF->rX;
	bodyPose_est_msg.pose.position.y = imuEKF->rY;
	bodyPose_est_msg.pose.position.z = imuEKF->rZ;
	bodyPose_est_msg.pose.orientation.x = imuEKF->qib_.x();
	bodyPose_est_msg.pose.orientation.y = imuEKF->qib_.y();
	bodyPose_est_msg.pose.orientation.z = imuEKF->qib_.z();
	bodyPose_est_msg.pose.orientation.w = imuEKF->qib_.w();
	bodyPose_est_pub.publish(bodyPose_est_msg);

	bodyVel_est_msg.header.stamp = ros::Time::now();
	bodyVel_est_msg.header.frame_id = "odom";
	bodyVel_est_msg.twist.linear.x = imuEKF->velX;
	bodyVel_est_msg.twist.linear.y = imuEKF->velY;
	bodyVel_est_msg.twist.linear.z = imuEKF->velZ;
	bodyVel_est_msg.twist.angular.x = imuEKF->gyroX;
	bodyVel_est_msg.twist.angular.y = imuEKF->gyroY;
	bodyVel_est_msg.twist.angular.z = imuEKF->gyroZ;
	bodyVel_est_pub.publish(bodyVel_est_msg);

	bodyAcc_est_msg.header.stamp = ros::Time::now();
	bodyAcc_est_msg.header.frame_id = "odom";
	bodyAcc_est_msg.linear_acceleration.x = imuEKF->accX;
	bodyAcc_est_msg.linear_acceleration.y = imuEKF->accY;
	bodyAcc_est_msg.linear_acceleration.z = imuEKF->accZ;
	bodyAcc_est_msg.angular_velocity.x = imuEKF->gyroX;
	bodyAcc_est_msg.angular_velocity.y = imuEKF->gyroY;
	bodyAcc_est_msg.angular_velocity.z = imuEKF->gyroZ;
	bodyAcc_est_pub.publish(bodyAcc_est_msg);


	//时间戳为0意味着滤波器还没开始
	if(imuEKF->timestamps!=0)
	{
		//odom_est_msg.header.stamp=ros::Time::now();
		ros::Time xx(imuEKF->timestamps);
		odom_est_msg.header.stamp=xx;
		odom_est_msg.header.frame_id = "odom";
		odom_est_msg.pose.pose = bodyPose_est_msg.pose;
		odom_est_msg.twist.twist = bodyVel_est_msg.twist;
		odom_est_pub.publish(odom_est_msg);
	}
}




void imu_estimator::advertise() {

	bodyPose_est_pub = n.advertise<geometry_msgs::PoseStamped>(
	"/imu_ekf/body/pose", 1000);


	bodyVel_est_pub = n.advertise<geometry_msgs::TwistStamped>(
	"/imu_ekf/body/vel", 1000);

	bodyAcc_est_pub = n.advertise<sensor_msgs::Imu>(
	"/imu_ekf/body/acc", 1000);

	odom_est_pub = n.advertise<nav_msgs::Odometry>("/imu_ekf/odom",1000);		
}





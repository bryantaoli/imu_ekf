kmsckf_V102.bag 是euroc数据集V102经过kmsckf跑出来的vio结果和原始imu数据结果

rosbag record /imu_ekf/odom
evo_traj bag 2020-03-22-11-49-37.bag /imu_ekf/odom --save_as_tum
evo_ape tum kmsckf_V102.tum odom.tum -va -s --plot


//后面这段重新从paramcontrol.cfg中读了一遍imu参数，把前面的都覆盖读了一遍，所以我注释掉了，这样改yaml文件里可以改了
	/*dynamic_recfg_ = boost::make_shared< dynamic_reconfigure::Server<imu_ekf::ParamControlConfig> >(n);
    dynamic_reconfigure::Server<imu_ekf::ParamControlConfig>::CallbackType cb = boost::bind(&imu_estimator::reconfigureCB, this, _1, _2);
    dynamic_recfg_->setCallback(cb);*/
把yaml文件中观测值的方差增加，会导致估计变差，因为这里面没有外参的矫正，所以只能提高观测值的方差使得观测值非常的稳定（这里外参矫正要找时间加进去）  而kmsckf输出的结果odom可能就是基于imu系的所以很可能外参就不用改（这个需要去确认），如果要改外参需要自己采的那个imu和lidar的数据进行实验。然后是姿态这里都是0有点弄不清楚

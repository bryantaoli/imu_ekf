kmsckf_V102.bag 是euroc数据集V102经过kmsckf跑出来的vio结果和原始imu数据结果

rosbag record /imu_ekf/odom
evo_traj bag 2020-03-22-11-49-37.bag /imu_ekf/odom --save_as_tum
evo_ape tum kmsckf_V102.tum odom.tum -va -s --plot

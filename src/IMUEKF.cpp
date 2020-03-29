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

#include "imu_ekf/IMUEKF.h"


void IMUEKF::init() 
{
	firstrun = true;
	P = Matrix<double, 15, 15>::Identity() * (1e-3);
	P(9, 9) = 1e-6;
	P(10, 10) = 1e-7;
	P(11, 11) = 1e-7;
	P(12, 12) = 1e-7;
	P(13, 13) = 1e-7;
	P(14, 14) = 1e-7;

	//Construct C
	Hf = Matrix<double, 6,15>::Zero();

	/*Initialize the state **/

	//Rotation Matrix from Inertial to body frame  也就是body系(imu系)和惯性系(世界系)开始是重合的
	Rib = Matrix3d::Identity();

	x = Matrix<double,15,1>::Zero();
	
	//x = 100*Matrix<double,15,1>::Ones();
	

	//Initializing w.r.t NAO Robot -- CHANGE IF NEEDED

	//Innovation Vector
	z = Matrix<double, 6, 1>::Zero();

	//Gravity Vector
	g = Vector3d::Zero();
	g(2) = -9.80665;

	//Initializing vectors and matrices
	r = Vector3d::Zero();
	v = Vector3d::Zero();
	chi = Vector3d::Zero();

	dxf = Matrix<double, 15, 1>::Zero();

	temp = Vector3d::Zero();
	tempM = Matrix3d::Zero();
	Kf = Matrix<double, 15, 6>::Zero();
	s = Matrix<double, 6, 6>::Zero();
	If = Matrix<double, 15, 15>::Identity();

	Acf = Matrix<double, 15, 15>::Zero();
	Lcf = Matrix<double, 15, 12>::Zero();
	Qff = Matrix<double, 15, 15>::Zero();
	Qf = Matrix<double, 12, 12>::Zero();
	Af = Matrix<double, 15, 15>::Zero();

	bw = Vector3d::Zero();
	bf = Vector3d::Zero();

	//bias removed acceleration and gyro rate
	fhat = Vector3d::Zero();
	omegahat = Vector3d::Zero();

	Tis = Affine3d::Identity();
	Tib = Affine3d::Identity();
	//Output Variables
	angleX = 0.000;
	angleY = 0.000;
	angleZ = 0.000;
	gyroX = 0.000;
	gyroY = 0.000;
	gyroZ = 0.000;
	accX = 0.000;
	accY = 0.000;
	accZ = 0.000;
	rX = 0.000;
	rY = 0.000;
	rZ = 0.000;
	velX = 0.000;
	velY = 0.000;
	velZ = 0.000;

	std::cout << "IMU EKF Initialized Successfully" << std::endl;

}

/** ------------------------------------------------------------- **/
/** IMU EKF filter to  deal with the Noise **/
void IMUEKF::predict(Vector3d omega_, Vector3d f_)
{
	omega = omega_;
	f = f_;

	// relative velocity
	v = x.segment<3>(0); //前三位是相对速度
	// absolute position
	r = x.segment<3>(6);//6到9是绝对位置
	// biases
	bw = x.segment<3>(9);
	bf = x.segment<3>(12);
	
	// Correct the inputs
	fhat = f - bf;
	omegahat = omega - bw;

	/** Linearization **/
	//Transition matrix Jacobian
	Acf.block<3,3>(0,0) = -wedge(omegahat); //改正的角速度读数的反对称矩阵
	Acf.block<3,3>(0,3) = wedge(Rib.transpose() * g);
	Acf.block<3,3>(3,3) = -wedge(omegahat);
	Acf.block<3,3>(6,0) = Rib;
	Acf.block<3,3>(6,3).noalias() = -Rib * wedge(v);
	Acf.block<3,3>(0,9) = -wedge(v);
	Acf.block<3,3>(0,12) = -Matrix3d::Identity();
	Acf.block<3,3>(3,9) = -Matrix3d::Identity();
	
	//State Noise Jacobian
	//gyro (0),acc (3),gyro_bias (6),acc_bias (9),foot_pos (12),foot_psi (15)		
	Lcf.block<3,3>(0,0) = wedge(v);
	Lcf.block<3,3>(0,3) = Matrix3d::Identity();
	Lcf.block<3,3>(3,0) = Matrix3d::Identity(); 	
	Lcf.block<3,3>(9,6) = Matrix3d::Identity();
	Lcf.block<3,3>(12,9) = Matrix3d::Identity();


	// Covariance Q with full state + biases
	Qf(0, 0) = GyroSTDx * GyroSTDx;
	Qf(1, 1) = GyroSTDy * GyroSTDy;
	Qf(2, 2) = GyroSTDz * GyroSTDz;
	Qf(3, 3) = AccSTDx * AccSTDx;
	Qf(4, 4) = AccSTDy * AccSTDy;
	Qf(5, 5) = AccSTDz * AccSTDz;
	Qf(6, 6) = GyroBiasSTDx * GyroBiasSTDx;
	Qf(7, 7) = GyroBiasSTDy * GyroBiasSTDy;
	Qf(8, 8) = GyroBiasSTDz * GyroBiasSTDz;
	Qf(9, 9) = AccBiasSTDx * AccBiasSTDx;
	Qf(10, 10) = AccBiasSTDy * AccBiasSTDy;
	Qf(11, 11) = AccBiasSTDz * AccBiasSTDz;
	

	//Euler Discretization - ZOH
	Af = If;
	Af.noalias() +=  Acf * dt;
	Qff.noalias() =  Af * Lcf * Qf * Lcf.transpose() * Af.transpose() * dt ;

	/** Predict Step: Propagate the Error Covariance  **/
	P = Af * P * Af.transpose() + Qff;
	
	/** Predict Step : Propagate the Mean estimate **/
	//Body Velocity
	temp = v.cross(omegahat) + Rib.transpose() * g + fhat;
	temp *= dt;
	
	x(0) = v(0) + temp(0); //速度
	x(1) = v(1) + temp(1);
	x(2) = v(2) + temp(2);

	x(3) = 0;
	x(4) = 0; //姿态怎么预测直接变0？
	x(5) = 0;

	//Body position
	temp = Rib * v; //位移
	temp *= dt;
	x(6) = r(0) + temp(0);
	x(7) = r(1) + temp(1);
	x(8) = r(2) + temp(2);

	//Gyro bias
	x(9)  = bw(0);
	x(10) = bw(1);
	x(11) = bw(2);

	//Acc bias
	x(12) = bf(0);
	x(13) = bf(1);
	x(14) = bf(2);


	//Propagate only if non-zero input 只在不是0输入的时候更新
	temp = omegahat;
	temp *= dt; // temp=omegahat*dt 也就是陀螺仪输入乘上dt就是角度变化,omegahat是一个三维向量
	if (temp(0) != 0.0000 && temp(1) != 0.0000 && temp(2) != 0.0000) {
		Rib  *=  expMap(temp, 1.0); //expMap是Rodriguez Formula,temp是陀螺仪输出的一个小角度，转成了矩阵
	}
	updateVars();
	//std::cout<<x<<std::endl;
}



void IMUEKF::updateWithOdom(Vector3d y, Quaterniond qy)
{
	Hf.noalias() = Matrix<double,6,15>::Zero();
	R(0, 0) = KinSTDx * KinSTDx;
	R(1, 1) = KinSTDy * KinSTDy;
	R(2, 2) = KinSTDz * KinSTDz;
	R(3, 3) = KinSTDOrientx * KinSTDOrientx;
	R(4, 4) = KinSTDOrienty * KinSTDOrienty;
	R(5, 5) = KinSTDOrientz * KinSTDOrientz;
	
	//std::cout<<"R="<<std::endl<<R<<std::endl;
	r = x.segment<3>(6);
	//Innovetion vector
	z.segment<3>(0).noalias() = y - r;
	Hf.block<3,3>(0,6) = Matrix3d::Identity();

	Quaterniond qib(Rib);
	z.segment<3>(3) = logMap( (qy * qib.inverse() ));
	//std::cout<<"z.segment<3>(3)="<<std::endl<<z.segment<3>(3)<<std::endl; // 第一个历元输出 0.0722031  -1.90441 -0.00799845

	Hf.block<3,3>(3,3) = Matrix3d::Identity();

	s = R;
	s.noalias() += Hf * P * Hf.transpose();
	Kf.noalias() = P * Hf.transpose() * s.inverse();

	dxf.noalias() = Kf * z;
	//std::cout<<"x="<<std::endl<<x<<std::endl;

	//Update the mean estimate
	x += dxf;
	//std::cout<<"dxf="<<std::endl<<dxf<<std::endl;
	//exit(0);
	//Update the error covariance
	P = (If - Kf * Hf) * P * (If - Kf * Hf).transpose() + Kf * R * Kf.transpose();

	if (dxf(3) != 0.000 && dxf(4) != 0.000 && dxf(5) != 0.000) {
		temp(0) = dxf(3);
		temp(1) = dxf(4);
		temp(2) = dxf(5);
		Rib *=  expMap(dxf.segment<3>(3), 1.0);
	}
	x.segment<3>(3) = Vector3d::Zero();
	updateVars();
}

		/** Update **/
void IMUEKF::updatewithVO(Vector3d y, Quaterniond qy){

	Hf.noalias() = Matrix<double,6,15>::Zero();

		R(0, 0) = KinSTDx * KinSTDx;
		R(1, 1) = KinSTDy * KinSTDy;
		R(2, 2) = KinSTDz * KinSTDz;
		R(3, 3) = KinSTDOrientx * KinSTDOrientx;
		R(4, 4) = KinSTDOrienty * KinSTDOrienty;
		R(5, 5) = KinSTDOrientz * KinSTDOrientz;
		R /=dt;
		r = x.segment<3>(6);

		//Innovetion vector
		z.segment<3>(0).noalias() = y - r;

		Hf.block<3,3>(0,6) = Matrix3d::Identity();

		Quaterniond qib(Rib);
		z.segment<3>(3) = logMap( (qy * qib.inverse() ));

		Hf.block<3,3>(3,3) = Matrix3d::Identity();

        s = R;
		s.noalias() += Hf * P * Hf.transpose();
		Kf.noalias() = P * Hf.transpose() * s.inverse();

		dxf.noalias() = Kf * z;

		//Update the mean estimate
		x += dxf;

		//Update the error covariance
		P = (If - Kf * Hf) * P * (If - Kf * Hf).transpose() + Kf * R * Kf.transpose();

		if (dxf(3) != 0.000 && dxf(4) != 0.000 && dxf(5) != 0.000) {
			temp(0) = dxf(3);
			temp(1) = dxf(4);
			temp(2) = dxf(5);
			Rib *=  expMap(dxf.segment<3>(3), 1.0);//这里是姿态更新
		}

		x.segment<3>(3) = Vector3d::Zero();//这里姿态又清零了，姿态用的是Rib

		updateVars();
}





		/** Update **/
void IMUEKF::updateWithTwist(Vector3d y, Quaterniond qy)
{
	Hf.noalias() = Matrix<double,6,15>::Zero();

	R(0, 0) = KinSTDx * KinSTDx;
	R(1, 1) = KinSTDy * KinSTDy;
	R(2, 2) = KinSTDz * KinSTDz;

	R(3, 3) = KinSTDOrientx * KinSTDOrientx;
	R(4, 4) = KinSTDOrienty * KinSTDOrienty;
	R(5, 5) = KinSTDOrientz * KinSTDOrientz;

	v = x.segment<3>(0);


	//Innovetion vector
	z.segment<3>(0).noalias() = y - Rib * v;


	Hf.block<3,3>(0,0) = Rib;
	Hf.block<3,3>(0,3).noalias() = -Rib * wedge(v);


	Quaterniond qib(Rib);
	//MAYBE qib.inverse() * qy BODY LOCAL PERTUBATION
	z.segment<3>(3) = logMap( (qy * qib.inverse() ));

	Hf.block<3,3>(3,3) = Matrix3d::Identity();


	s = R;
	s.noalias() += Hf * P * Hf.transpose();
	Kf.noalias() = P * Hf.transpose() * s.inverse();

	dxf.noalias() = Kf * z;

	//Update the mean estimate
	x += dxf;

	//Update the error covariance
	P = (If - Kf * Hf) * P * (If - Kf * Hf).transpose() + Kf * R * Kf.transpose();


	if (dxf(3) != 0.000 && dxf(4) != 0.000 && dxf(5) != 0.000) {
		temp(0) = dxf(3);
		temp(1) = dxf(4);
		temp(2) = dxf(5);
		Rib *=  expMap(dxf.segment<3>(3), 1.0);
	}
	x.segment<3>(3) = Vector3d::Zero();

	updateVars();
}




void IMUEKF::updateVars()
{
	updateTF();

	//Update the biases
	biasGX = x(9);
	biasGY = x(10);
	biasGZ = x(11);
	biasAX = x(12);
	biasAY = x(13);
	biasAZ = x(14);

	omegahat.noalias() = omega - Vector3d(x(9), x(10), x(11));
	fhat.noalias() = f - Vector3d(x(12), x(13), x(14));

	temp.noalias() = Rib * omegahat;
	gyroX = temp(0);
	gyroY = temp(1);
	gyroZ = temp(2);

	temp.noalias() =  Rib * fhat;
	accX = temp(0);
	accY = temp(1);
	accZ = temp(2);
	velX = x(0);
	velY = x(1);
	velZ = x(2);

	rX = x(6);
	rY = x(7);
	rZ = x(8);

	temp = getEulerAngles(Rib);
	angleX = temp(0);
	angleY = temp(1);
	angleZ = temp(2);
}

void IMUEKF::updateTF() 
{
	Tib.linear() = Rib;
	Tib.translation() = r;
	qib_ = Quaterniond(Tib.linear());
}

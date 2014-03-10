/*
 * image_filter.h
    Copyright (C) 2013  Timothy Sweet

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <ros/ros.h>
#include "dhs/ImageFetcher.h"
#include "dhs/ImageSender.h"

#define NO_THRESHOLDING -1
#define DEFAULT_KERNEL 15

class Worker {
public:
	Worker(const std::string& input_topic, const std::string& output_topic) :
		handle_("~"),
		input_stream_(input_topic),
		output_stream_(output_topic) {
		if(!handle_.getParam("threshold", threshold_))
			threshold_ = NO_THRESHOLDING;
		int kernel;
		if(!handle_.getParam("gaussian_kernel", kernel))
			kernel = DEFAULT_KERNEL;
		kernel_.height = kernel;
		kernel_.width = kernel;
	}

	void callback(const ros::TimerEvent& event);
private:
	ros::NodeHandle handle_;
	ImageFetcher input_stream_;
	ImageSender output_stream_;
	int threshold_;
	cv::Size kernel_;
};

int main(int argc, char* argv[]) {
	//connect to ros
	ros::init(argc, argv, "image_filter");
	ros::NodeHandle handle("~");

	Worker worker("in","out");
	//loop at 120 hz since the camera runs quarter that fast
	ros::Timer timer = handle.createTimer(ros::Duration(1./120.), &Worker::callback, &worker);

	ros::spin();

}

void Worker::callback(const ros::TimerEvent& event) {
	//if there is a subscriber
	if(output_stream_.HasSubscriber()) {
		//if the frame has been updated
		//aka sequence number is not zero
		cv::Mat frame;
		int sequence_number = input_stream_.GetFrame(frame);

		if(sequence_number!=FRAME_NOT_UPDATED) {
			//do thresholding if set
			if(threshold_!=NO_THRESHOLDING) {
				//threshold values higher than threshold to 0
				cv::threshold(frame,frame,threshold_,0,cv::THRESH_TOZERO_INV);
			}
			cv::GaussianBlur(frame,frame,kernel_,0);
			output_stream_.SendFrame(sequence_number,frame);
		}
	}
}

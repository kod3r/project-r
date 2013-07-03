#include <ros/ros.h>
#include "MessageFetcher.h"
#include "Analysis.h"
#include <iostream>
#include <string>
#include <fstream>

#define HIST_HSV_H 0

static const std::string kWindow = "output";
static const std::string kDepthWindow = "depth";
static const int kFontThickness = 2;
static const double kFontScale = 0.6;
static const std::string kUpperPicker = "Upper Color Picker";
static const std::string kLowerPicker = "Lower Color Picker";



static const bool kRecord = true;


struct picker_helper {
	cv::Scalar value;
	cv::Mat map;
	cv::Mat original_map;
	bool color_set;
	picker_helper() {
		color_set = false;
	}
};



void drawBlobs(const std::vector<BlobDescriptor>& blobs,cv::Mat* image, int frame_number);
int getVideoNumber();
void displayHistogram(const cv::MatND& histogram,int type);
cv::Point calcCenter(const cv::Rect& rectangle);
void SetupPicker(picker_helper* upper,picker_helper* lower);
static void UpperPickerCallback( int event, int x, int y, int, void* );
static void LowerPickerCallback( int event, int x, int y, int, void* );
void FindUserRequest(const std::vector<BlobDescriptor>& blobs, const picker_helper& upper,const picker_helper& lower,cv::Mat *color_raw,int frame_number);


int main(int argc, char** argv)
{
	ros::init(argc, argv, "imageProcessor");

	MessageFetcher ros_handle;
	cv::Mat color_raw,depth_raw;
	Analysis analysis_handle;
	std::vector<BlobDescriptor> blobs;
	
	bool run = true;
	int frame_number;
	
	//create output writer
	cv::VideoWriter writer;
	std::stringstream video_output_name;
	video_output_name << "saves/save_"<<getVideoNumber()<<".avi";
	if(kRecord) writer.open(video_output_name.str(),CV_FOURCC('D','I','V','X'),25,cv::Size(640,480),true);

	//setup everything
	picker_helper upper_picker,lower_picker;
	SetupPicker(&upper_picker,&lower_picker);

	std::cout<<"Running"<<std::endl;

	//discard first frame for depth segmentation reasons
	while(!ros_handle.GetFrame(color_raw,depth_raw) && ros::ok()) {
		ros::spinOnce();
	}
	if(ros::ok()) frame_number = analysis_handle.AddFrame(color_raw,depth_raw);

	//main loop
	while(ros::ok() && run) {
		//spin until we get a new frame
		if(!ros_handle.GetFrame(color_raw,depth_raw)) {
			ros::spinOnce();
			continue;
		}

		//add the frame to the segmentation
		frame_number = analysis_handle.AddFrame(color_raw,depth_raw);
		//imshow("color foreground",analysis_handle.color_foreground());
		//imshow("depth foreground",analysis_handle.depth_foreground());
		//imshow("combined foreground",analysis_handle.combined_foreground());
		analysis_handle.UpdateBlobs(&blobs);

		//show the blobs on the frame
		drawBlobs(blobs,&color_raw,frame_number);

		//look for the user-selected colors
//		FindUserRequest(blobs,upper_picker,lower_picker,&color_raw,frame_number);

		//save the video, if applicable
		if(kRecord) writer.write(color_raw);

		//show the image on screen
		imshow(kWindow,color_raw);
//		imshow(kDepthWindow,depth_raw);


		char key = cv::waitKey(1);
		switch(key) {
		case 'q':
		case 'Q':
			cv::destroyAllWindows();
			run = false;
			break;
		}
	}

	return 0;
}

void drawBlobs(const std::vector<BlobDescriptor>& blobs,cv::Mat* image, int frame_number) {
	int blobs_drawn = 0;
	for(unsigned int blob_index=0;blob_index<blobs.size();blob_index++) {
		std::stringstream text;
		text << blob_index;
		if(blobs[blob_index].last_seen==frame_number) {
			//show the blob in a separate window
//			imshow(text.str(),blobs[blob_index].image);

			//put diagnostic text onscreen
			cv::Point textOrigin(blobs[blob_index].last_location.x,blobs[blob_index].last_location.y);
			cv::putText(*image,text.str(),textOrigin,CV_FONT_HERSHEY_SIMPLEX,kFontScale,cv::Scalar(255,0,0),kFontThickness,8);
			cv::rectangle(*image,blobs[blob_index].last_location,cv::Scalar(blob_index*20%256,blob_index*20%256,blob_index*20%256),1);
			std::cout<<blob_index<<" history size: "<<blobs[blob_index].history.size()<<std::endl;
			//draw a line showing bag ownership
			if(blobs[blob_index].belongs_to!=-1) {
				int owner_index = blobs[blob_index].belongs_to;
				cv::Point owner_center = calcCenter(blobs[owner_index].last_location);
				cv::Point bag_center = calcCenter(blobs[blob_index].last_location);

				cv::line(*image,owner_center,bag_center,cv::Scalar(255,0,0),2,8);
			}

			//increment counter
			blobs_drawn++;
		}
		//draw Kalman estimates
		int x = blobs[blob_index].location_estimate.at<float>(0);
		x -= blobs[blob_index].dimension_estimate.at<float>(0)/2;
		int y = blobs[blob_index].location_estimate.at<float>(1);
		y -= blobs[blob_index].dimension_estimate.at<float>(1)/2;
		cv::Point kalman_estimate_origin(x,y);
		cv::putText(*image,text.str(),kalman_estimate_origin,CV_FONT_HERSHEY_SIMPLEX,kFontScale,cv::Scalar(0,0,255),kFontThickness,8);
		cv::Size kalman_estimate_size(blobs[blob_index].dimension_estimate.at<float>(0),blobs[blob_index].dimension_estimate.at<float>(1));
		cv::Rect kalman_estimate(kalman_estimate_origin,kalman_estimate_size);

		cv::rectangle(*image,kalman_estimate,cv::Scalar(0,0,255),3,8);
	}
}
int getVideoNumber() {
	std::ifstream fin;
	fin.clear();
	fin.open("saves/video_number");
	int number;
	if(fin.good()) {
		fin >> number;
		fin.close();
	}
	else {
		number = 0;
	}
	std::ofstream fout;
	fout.clear();
	fout.open("saves/video_number");
	fout << (number+1);
	fout.close();
	return number;
}

void displayHistogram(const cv::MatND& histogram,int type) {
	std::cout<<histogram.rows<<std::endl<<histogram<<std::endl;
	cv::Mat image(100,histogram.rows,CV_8UC3);
	uchar* data = histogram.data;
	if(type==HIST_HSV_H) {
		for(int i=0;i<histogram.rows;i++) {
			cv::line(image,cv::Point(i,99),cv::Point(i,99-*data),cv::Scalar(i,128,128),1,8);
		data++;
		}
	}
	cv::cvtColor(image,image,CV_RGB2HSV);
	imshow("histogram",image);
}

cv::Point calcCenter(const cv::Rect& rectangle) {
	cv::Point center(rectangle.x+rectangle.width/2,rectangle.y+rectangle.height/2);
	return center;
}
void SetupPicker(picker_helper* upper,picker_helper* lower) {
	cv::namedWindow(kUpperPicker,CV_WINDOW_AUTOSIZE);
	cv::namedWindow(kLowerPicker,CV_WINDOW_AUTOSIZE);

	lower->map = cv::imread("color_map.png");
	upper->map = cv::imread("color_map.png");

	lower->map.copyTo(lower->original_map);
	upper->map.copyTo(upper->original_map);
	cv::setMouseCallback(kUpperPicker,UpperPickerCallback,upper);
	cv::setMouseCallback(kLowerPicker,LowerPickerCallback,lower);
	imshow(kUpperPicker,upper->map);
	imshow(kLowerPicker,lower->map);

}

static void UpperPickerCallback( int event, int x, int y, int, void* input ) {
	picker_helper* data = (picker_helper*) input;

	if(event == cv::EVENT_LBUTTONDOWN) {
		data->color_set = !data->color_set;

	}
	else if(event == cv::EVENT_MOUSEMOVE){
		if(!data->color_set) {
			data->original_map.copyTo(data->map);
			data->value = cv::Scalar(data->map.at<cv::Vec3b>(y,x)[0],data->map.at<cv::Vec3b>(y,x)[1],data->map.at<cv::Vec3b>(y,x)[2]);
			cv::circle(data->map,cv::Point(x,y),16,cv::Scalar(0,0,0),2,8);
			cv::circle(data->map,cv::Point(x,y),15,data->value,-1,8);
			imshow(kUpperPicker,data->map);
		}
	}
}

static void LowerPickerCallback( int event, int x, int y, int, void* input ) {
	picker_helper* data = (picker_helper*) input;

	if(event == cv::EVENT_LBUTTONDOWN) {
		data->color_set = !data->color_set;

	}
	else if(event == cv::EVENT_MOUSEMOVE){
		if(!data->color_set) {
			data->original_map.copyTo(data->map);
			data->value = cv::Scalar(data->map.at<cv::Vec3b>(y,x)[0],data->map.at<cv::Vec3b>(y,x)[1],data->map.at<cv::Vec3b>(y,x)[2]);
			cv::circle(data->map,cv::Point(x,y),16,cv::Scalar(0,0,0),2,8);
			cv::circle(data->map,cv::Point(x,y),15,data->value,-1,8);
			imshow(kLowerPicker,data->map);
		}
	}
}

void FindUserRequest(const std::vector<BlobDescriptor>& blobs, const picker_helper& upper,const picker_helper& lower,cv::Mat *color_raw,int frame_number) {
	for(unsigned int blob_index=0;blob_index<blobs.size();blob_index++) {
		//if the blob is visible in this frame and not a bag
		if(blobs[blob_index].last_seen==frame_number && blobs[blob_index].belongs_to==NO_OWNER) {
			//compare the upper and lower regions
				//make a histogram for the user upper and lower
				cv::Size upper_size(blobs[blob_index].last_location.width,blobs[blob_index].last_location.height*0.4);
				cv::Size lower_size(blobs[blob_index].last_location.width,blobs[blob_index].last_location.height*0.6);

				cv::Mat upper_hsv(upper_size,CV_8UC3,upper.value),
						lower_hsv(lower_size,CV_8UC3,lower.value);
				cv::cvtColor(upper_hsv,upper_hsv,CV_BGR2HSV);
				cv::cvtColor(lower_hsv,lower_hsv,CV_BGR2HSV);

				std::vector<cv::Mat> upper_planes,
									 lower_planes;
				cv::split(upper_hsv,upper_planes);
				cv::split(lower_hsv,lower_planes);

				cv::MatND user_upper,user_lower;
				int histSize = 180;
				float hist_range[] = {1,180};
				const float* ranges = {hist_range};
				cv::calcHist(&upper_planes[0],1,0,cv::Mat(),user_upper,1,&histSize,&ranges,true,false);
				double relation = cv::compareHist(user_upper,blobs[blob_index].upper,CV_COMP_CORREL);
				if(relation > 0) {
					//draw a big elipse around this blob
					cv::rectangle(*color_raw,blobs[blob_index].last_location,cv::Scalar(255,0,0),5,8);
				}
//				std::cout<<"upper planes histogram: "<<user_upper<<std::endl;
//				std::cout<<"Upper for blob "<<blob_index<<": "<<blobs[blob_index].upper<<std::endl;
		}
	}
}

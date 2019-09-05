// Copyright 2018 Slightech Co., Ltd. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <opencv2/calib3d/calib3d.hpp>

#include "camera.h"

MYNTEYE_BEGIN_NAMESPACE

namespace models {

Camera::Parameters::Parameters(ModelType modelType)
    : m_modelType(modelType), m_imageWidth(0), m_imageHeight(0) {
  switch (modelType) {
    case KANNALA_BRANDT:
      m_nIntrinsics = 8;
      break;
    case PINHOLE:
      m_nIntrinsics = 8;
      break;
    case MEI:
    default:
      m_nIntrinsics = 9;
  }
}

Camera::Parameters::Parameters(
    ModelType modelType, const std::string &cameraName, int w, int h)
    : m_modelType(modelType),
      m_cameraName(cameraName),
      m_imageWidth(w),
      m_imageHeight(h) {
  switch (modelType) {
    case KANNALA_BRANDT:
      m_nIntrinsics = 8;
      break;
    case PINHOLE:
      m_nIntrinsics = 8;
      break;
    case MEI:
    default:
      m_nIntrinsics = 9;
  }
}

Camera::ModelType &Camera::Parameters::modelType(void) {
  return m_modelType;
}

std::string &Camera::Parameters::cameraName(void) {
  return m_cameraName;
}

int &Camera::Parameters::imageWidth(void) {
  return m_imageWidth;
}

int &Camera::Parameters::imageHeight(void) {
  return m_imageHeight;
}

Camera::ModelType Camera::Parameters::modelType(void) const {
  return m_modelType;
}

const std::string &Camera::Parameters::cameraName(void) const {
  return m_cameraName;
}

int Camera::Parameters::imageWidth(void) const {
  return m_imageWidth;
}

int Camera::Parameters::imageHeight(void) const {
  return m_imageHeight;
}

int Camera::Parameters::nIntrinsics(void) const {
  return m_nIntrinsics;
}

cv::Mat &Camera::mask(void) {
  return m_mask;
}

const cv::Mat &Camera::mask(void) const {
  return m_mask;
}


void Camera::estimateExtrinsics(
    const std::vector<cv::Point3f> &objectPoints,
    const std::vector<cv::Point2f> &imagePoints, cv::Mat &rvec,
    cv::Mat &tvec) const {
  std::vector<cv::Point2f> Ms(imagePoints.size());
  for (size_t i = 0; i < Ms.size(); ++i) {
  // Eigen::Vector3d P;
  ctain::Vectord P(3, 1), p(2, 1);
  p<< imagePoints.at(i).x << imagePoints.at(i).y;

  // liftProjective(
  // Eigen::Vector2d(imagePoints.at(i).x, imagePoints.at(i).y), P);
  liftProjective(p, P);
  P = P / P(2);
  Ms.at(i).x = P(0);
  Ms.at(i).y = P(1);
  }

  // assume unit focal length, zero principal point, and zero distortion
  cv::solvePnP(
      objectPoints, Ms, cv::Mat::eye(3, 3, CV_64F), cv::noArray(), rvec, tvec);
}

double Camera::reprojectionDist(
    const ctain::Vector3d &P1, const ctain::Vector3d &P2) const {
  ctain::Vector2d p1(2, 1), p2(2, 1);

  spaceToPlane(P1, p1);
  spaceToPlane(P2, p2);

  return (p1 - p2).norm();
}

double Camera::reprojectionError(
    const std::vector<std::vector<cv::Point3f> > &objectPoints,
    const std::vector<std::vector<cv::Point2f> > &imagePoints,
    const std::vector<cv::Mat> &rvecs, const std::vector<cv::Mat> &tvecs,
    cv::OutputArray _perViewErrors) const {
  int imageCount = objectPoints.size();
  size_t pointsSoFar = 0;
  double totalErr = 0.0;

  bool computePerViewErrors = _perViewErrors.needed();
  cv::Mat perViewErrors;
  if (computePerViewErrors) {
    _perViewErrors.create(imageCount, 1, CV_64F);
    perViewErrors = _perViewErrors.getMat();
  }

  for (int i = 0; i < imageCount; ++i) {
    size_t pointCount = imagePoints.at(i).size();

    pointsSoFar += pointCount;

    std::vector<cv::Point2f> estImagePoints;
    projectPoints(objectPoints.at(i), rvecs.at(i), tvecs.at(i), estImagePoints);

    double err = 0.0;
    for (size_t j = 0; j < imagePoints.at(i).size(); ++j) {
      err += cv::norm(imagePoints.at(i).at(j) - estImagePoints.at(j));
    }

    if (computePerViewErrors) {
      perViewErrors.at<double>(i) = err / pointCount;
    }

    totalErr += err;
  }

  return totalErr / pointsSoFar;
}

double Camera::reprojectionError(
    const ctain::Vector3d &P, const ctain::Quaterniond &camera_q,
    const ctain::Vector3d &camera_t,
    const ctain::Vector2d &observed_p) const {
  ctain::Vector3d P_cam;
  P_cam = camera_q.toRotationMatrix() * P + camera_t;

  ctain::Vector2d p(2, 1), res(2, 1);
  spaceToPlane(P_cam, p);
  res = p - observed_p;
  return res.norm();
}

void Camera::projectPoints(
    const std::vector<cv::Point3f> &objectPoints, const cv::Mat &rvec,
    const cv::Mat &tvec, std::vector<cv::Point2f> &imagePoints) const {
  // project 3D object points to the image plane
  imagePoints.reserve(objectPoints.size());

  // double
  cv::Mat R0;
  cv::Rodrigues(rvec, R0);

  ctain::MatrixXd R(3, 3);
  R << R0.at<double>(0, 0) << R0.at<double>(0, 1) << R0.at<double>(0, 2) <<
       R0.at<double>(1, 0) << R0.at<double>(1, 1) << R0.at<double>(1, 2) <<
       R0.at<double>(2, 0) << R0.at<double>(2, 1) << R0.at<double>(2, 2);

  ctain::Vectord t(3, 1);
  t << tvec.at<double>(0) << tvec.at<double>(1) << tvec.at<double>(2);

  for (size_t i = 0; i < objectPoints.size(); ++i) {
    const cv::Point3f &objectPoint = objectPoints.at(i);

    // Rotate and translate
    ctain::Vectord P(3, 1);
    P << objectPoint.x << objectPoint.y << objectPoint.z;

    P = R * P + t;

    ctain::Vector2d p(2, 1);
    spaceToPlane(P, p);

    imagePoints.push_back(cv::Point2f(p(0), p(1)));
  }
}

}  // namespace models

MYNTEYE_END_NAMESPACE

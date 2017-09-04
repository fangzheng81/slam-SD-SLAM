/**
 *
 *  Copyright (C) 2017 Eduardo Perdices <eperdices at gsyc dot es>
 *
 *  The following code is a derivative work of the code from the ORB-SLAM2 project,
 *  which is licensed under the GNU Public License, version 3. This code therefore
 *  is also licensed under the terms of the GNU Public License, version 3.
 *  For more information see <https://github.com/raulmur/ORB_SLAM2>.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "FrameDrawer.h"
#include "Config.h"

using std::vector;
using std::mutex;

namespace SD_SLAM {

FrameDrawer::FrameDrawer(Map* pMap):mpMap(pMap) {
  mState=Tracking::SYSTEM_NOT_READY;
  mIm = cv::Mat(Config::Height(),Config::Width(),CV_8UC3, cv::Scalar(0,0,0));
}

cv::Mat FrameDrawer::DrawFrame() {
  cv::Mat im;
  vector<cv::KeyPoint> vIniKeys; // Initialization: KeyPoints in reference frame
  vector<int> vMatches; // Initialization: correspondeces with reference keypoints
  vector<cv::KeyPoint> vCurrentKeys; // KeyPoints in current frame
  vector<bool> vbMap; // Tracked MapPoints in current frame
  int state; // Tracking state

  //Copy variables within scoped mutex
  {
    std::unique_lock<mutex> lock(mMutex);
    state=mState;
    if (mState==Tracking::SYSTEM_NOT_READY)
      mState=Tracking::NO_IMAGES_YET;

    mIm.copyTo(im);

    if (mState==Tracking::NOT_INITIALIZED) {
      vCurrentKeys = mvCurrentKeys;
      vIniKeys = mvIniKeys;
      vMatches = mvIniMatches;
    } else if (mState==Tracking::OK) {
      vCurrentKeys = mvCurrentKeys;
      vbMap = mvbMap;
    } else if (mState==Tracking::LOST) {
      vCurrentKeys = mvCurrentKeys;
    }
  }

  if (im.channels()<3) //this should be always true
    cvtColor(im,im,CV_GRAY2BGR);

  //Draw
  if (state==Tracking::NOT_INITIALIZED) { //INITIALIZING
    for (unsigned int i=0; i<vMatches.size(); i++) {
      if (vMatches[i]>=0)
        cv::line(im, vIniKeys[i].pt, vCurrentKeys[vMatches[i]].pt, cv::Scalar(0,255,0), 2);
    }    
  } else if (state==Tracking::OK) { //TRACKING
    mnTracked=0;
    const float r = 3;
    const int n = vCurrentKeys.size();
    for (int i=0;i<n;i++) {
      if (vbMap[i]) {
        // This is a match to a MapPoint in the map
        cv::circle(im, vCurrentKeys[i].pt, r, cv::Scalar(0,255,0), 2);
        mnTracked++;
      }
    }
  }

  cv::Mat imWithInfo;
  DrawTextInfo(im,state, imWithInfo);

  return imWithInfo;
}


void FrameDrawer::DrawTextInfo(cv::Mat &im, int nState, cv::Mat &imText) {
  std::stringstream s;
  if (nState==Tracking::NO_IMAGES_YET)
    s << " WAITING FOR IMAGES";
  else if (nState==Tracking::NOT_INITIALIZED)
    s << " TRYING TO INITIALIZE ";
  else if (nState==Tracking::OK) {
    int nKFs = mpMap->KeyFramesInMap();
    int nMPs = mpMap->MapPointsInMap();
    s << "KFs: " << nKFs << ", MPs: " << nMPs << ", Matches: " << mnTracked;
  } else if (nState==Tracking::LOST) {
    s << " TRACK LOST. TRYING TO RELOCALIZE ";
  } else if (nState==Tracking::SYSTEM_NOT_READY) {
    s << " LOADING ORB VOCABULARY. PLEASE WAIT...";
  }

  int baseline=0;
  cv::Size textSize = cv::getTextSize(s.str(),cv::FONT_HERSHEY_PLAIN,1,1,&baseline);

  im.copyTo(imText);
  imText.rowRange(imText.rows-(textSize.height+10),imText.rows) = cv::Mat::zeros(textSize.height+10,im.cols,im.type());
  cv::putText(imText,s.str(),cv::Point(5,imText.rows-5),cv::FONT_HERSHEY_PLAIN,1,cv::Scalar(255,255,255),1,8);
}

void FrameDrawer::Update(Tracking *pTracker) {
  std::unique_lock<mutex> lock(mMutex);
  pTracker->mImGray.copyTo(mIm);
  mvCurrentKeys=pTracker->mCurrentFrame.mvKeys;
  N = mvCurrentKeys.size();
  mvbMap = vector<bool>(N,false);

  if (pTracker->mLastProcessedState==Tracking::NOT_INITIALIZED)  {
    mvIniKeys=pTracker->mInitialFrame.mvKeys;
    mvIniMatches=pTracker->mvIniMatches;
  } else if (pTracker->mLastProcessedState==Tracking::OK) {
    for (int i=0;i<N;i++) {
      MapPoint* pMP = pTracker->mCurrentFrame.mvpMapPoints[i];
      if (pMP) {
        if (!pTracker->mCurrentFrame.mvbOutlier[i])
          mvbMap[i]=true;
      }
    }
  }
  mState=static_cast<int>(pTracker->mLastProcessedState);
}

}  // namespace SD_SLAM

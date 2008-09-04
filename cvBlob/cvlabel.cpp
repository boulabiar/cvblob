/***************************************************************************
 *   Copyright (C) 2007 by Cristóbal Carnero Liñán                         *
 *   grendel.ccl@gmail.com                                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <stdexcept>
#include <iostream>
using namespace std;

#ifdef WIN32
#include <cv.h>
#else
#include <opencv/cv.h>
#endif

#include "cvblob.h"

#define _MIN_(a,b) ((a)<(b)?(a):(b))
#define _MAX_(a,b) ((a)>(b)?(a):(b))

//#define NULL 0L

void makeSet(CvBlob *x)
{
  x->_parent=NULL;
  x->_rank=0;
}

CvBlob *find(CvBlob *x)
{
  if (!x->_parent) return x;
  else
  {
    x->_parent=find(x->_parent);
    return x->_parent;
  }
}

CvBlob *merge(CvBlob *x, CvBlob *y)
{
  CvBlob *xRoot=find(x);
  CvBlob *yRoot=find(y);
  
  if (xRoot->_rank > yRoot->_rank)
    yRoot->_parent=xRoot;
  else if (xRoot->_rank < yRoot->_rank)
    xRoot->_parent=yRoot;
  else if (xRoot!=yRoot)
  {
    yRoot->_parent=xRoot;
    xRoot->_rank+=1;
  }
}

unsigned int cvLabel (IplImage *img, IplImage *imgOut, CvBlobs &blobs)
{
  int numPixels=0;
  
  if((img->depth!=IPL_DEPTH_8U)||(img->nChannels!=1))
  {
    cerr<<"Error: Input image format."<<endl;
    return 0; /// TODO: Errores.
  }
  
  if((imgOut->depth!=IPL_DEPTH_LABEL)||(img->nChannels!=1))
  {
    cerr<<"Error: Output image format."<<endl;
    return 0; /// TODO: Errores.
  }

  //IplImage *imgOut=cvCreateImage (cvGetSize(img),IPL_DEPTH_LABEL,1);
  cvSetZero(imgOut);
  
  CvLabel label=0;
  blobs.clear();
  
  char *lastRowIn=img->imageData;
  CvLabel *lastRowOut=(CvLabel *)imgOut->imageData;
  char *imgDataIn=img->imageData+img->width;
  CvLabel *imgDataOut=(CvLabel *)imgOut->imageData+imgOut->width;
  // Skip first line!
  for (unsigned int r=1;r<img->height;r++,
      lastRowIn+=img->width,lastRowOut+=imgOut->width,imgDataIn+=img->width,imgDataOut+=imgOut->width)
    for (unsigned int c=1;c<img->width;c++)
    {
      if (imgDataIn[c])
      {
        numPixels++;
        if (lastRowOut[c])
        {
          CvBlob *blob=blobs[lastRowOut[c]];
          blob->area+=1;
          blob->maxy=_MAX_(blob->maxy,r);
          blob->m10+=c; blob->m01+=r;
          blob->m11+=c*r;
          blob->m20+=c*c; blob->m02+=r*r;
          
          imgDataOut[c]=lastRowOut[c];
          
          if ((imgDataOut[c-1])&&(imgDataOut[c]!=imgDataOut[c-1]))
          {
            CvBlob *blob1=blobs[imgDataOut[c]];
            CvBlob *blob2=blobs[imgDataOut[c-1]];
            
            merge(blob1,blob2);
          }
        }
        else if (imgDataOut[c-1])
        {
          CvBlob *blob=blobs[imgDataOut[c-1]];
          blob->area+=1;
          blob->maxx=_MAX_(blob->maxx,c);
          blob->m10+=c; blob->m01+=r;
          blob->m11+=c*r;
          blob->m20+=c*c; blob->m02+=r*r;
          
          imgDataOut[c]=imgDataOut[c-1];
        }
        else
        {
          label++;
          
          CvBlob *blob=new CvBlob;
          makeSet(blob);
          blob->label=label;
          blob->area=1;
          blob->minx=c; blob->maxx=c;
          blob->miny=r; blob->maxy=r;
          blob->m10=c; blob->m01=r;
          blob->m11=c*r;
          blob->m20=c*c; blob->m02=r*r;
          blobs.insert(CvLabelBlob(label,blob));
          
          imgDataOut[c]=label;
        }
      }
    }
  
  unsigned int labelSize=blobs.size();
  CvLabel *luLabels=new CvLabel[labelSize+1];
  luLabels[0]=0;
  
  for (CvBlobs::iterator it=blobs.begin();it!=blobs.end();++it)
  {
    CvBlob *blob1=(*it).second;
    CvBlob *blob2=find(blob1);
    
    blob2->area+=blob1->area;
    blob2->minx=_MIN_(blob2->minx,blob1->minx); blob2->maxx=_MAX_(blob2->maxx,blob1->maxx);
    blob2->miny=_MIN_(blob2->miny,blob1->miny); blob2->maxy=_MAX_(blob2->maxy,blob1->maxy);
    blob2->m10+=blob1->m10; blob2->m01+=blob1->m01;
    blob2->m11+=blob1->m11;
    blob2->m20+=blob1->m20; blob2->m02+=blob1->m02;
    
    luLabels[(*it).first]=blob2->label;
  }
  
  imgDataOut=(CvLabel *)imgOut->imageData+imgOut->width;
  // Skip first line!
  for (int r=1;r<imgOut->height;r++,imgDataOut+=imgOut->width)
    for (int c=1;c<imgOut->width;c++)
      imgDataOut[c]=luLabels[imgDataOut[c]];
  
  delete luLabels;
  
  // Eliminar los blobs hijos:
  CvBlobs::iterator it=blobs.begin();
  while (it!=blobs.end())
  {
    CvBlob *blob=(*it).second;
    if (blob->_parent)
    {
      delete blob;
      CvBlobs::iterator tmp=it;
      ++it;
      blobs.erase(tmp);
    }
    else
    {
      cvCentroid((*it).second); // Here?
      ++it;
    }
  }
  
  return numPixels;
}

// IplImage *cvFilterLabel(IplImage *imgIn, CvLabel label)
// {
//   if ((imgIn->depth!=IPL_DEPTH_LABEL)||(imgIn->nChannels!=1))
//   {
//     cerr<<"Error: Image format."<<endl;
//     return 0L; /// TODO: Errores.
//   }
//   
//   IplImage *imgOut=cvCreateImage (cvGetSize(imgIn),IPL_DEPTH_8U,1);
//   cvSetZero(imgOut);
//   
//   char *imgDataOut=imgOut->imageData+imgOut->width;
//   CvLabel *imgDataIn=(CvLabel *)imgIn->imageData+imgIn->width;
//   for (unsigned int r=1;r<imgIn->height;r++,
//        imgDataIn+=imgIn->width,imgDataOut+=imgOut->width)
//     for (unsigned int c=1;c<imgIn->width;c++)
//       if (imgDataIn[c]==label) imgDataOut[c]=0xff;
//   
//   return imgOut;
// }

void cvFilterLabels(IplImage *imgIn, IplImage *imgOut, CvBlobs blobs)
{
  if ((imgIn->depth!=IPL_DEPTH_LABEL)||(imgIn->nChannels!=1))
    throw logic_error("Input image format.");
  
  if ((imgOut->depth!=IPL_DEPTH_8U)||(imgOut->nChannels!=1))
    throw logic_error("Input image format.");
  
  char *imgDataOut=imgOut->imageData+imgOut->width;
  CvLabel *imgDataIn=(CvLabel *)imgIn->imageData+imgIn->width;
  for (unsigned int r=1;r<imgIn->height;r++,
       imgDataIn+=imgIn->width,imgDataOut+=imgOut->width)
  {
    for (unsigned int c=1;c<imgIn->width;c++)
    {
      if (imgDataIn[c])
      {
        if (blobs.find(imgDataIn[c])==blobs.end()) imgDataOut[c]=0x00;
        else imgDataOut[c]=0xff;
      }
    }
  }
}

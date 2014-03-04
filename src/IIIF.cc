/*
IIIF Request Command Handler Class Member Function
Author: Michal Becak

Copyright (C) 2012 Klokan Technologies GmbH (http://www.klokantech.com/).

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <algorithm>
#include <cmath>
#include <sstream>
#include "Environment.h"
#include "Task.h"
#include "Tokenizer.h"
#include "Transforms.h"

#if _MSC_VER
#include "../windows/Time.h"
#endif

using namespace std;

// The argument is in the form {identifier}/{region}/{size}/{rotation}/{quality}{.format}
// eg. filename.jp2/full/full/0/native.jpg
// or in the form {identifier}/info{.xml/.json} eg. filename.jp2/info.xml

void IIIF::run( Session* session, const std::string& argument ){

  if( session->loglevel >= 3 ) *(session->logfile) << "IIIF handler reached" << endl;

  // Time this command
  if( session->loglevel >= 2 ) command_timer.start();

  //decode URL
  string decodedArgument = FIF::decodeUrl(argument);

  //variables to store information from url request
  //suffix is last parameter eg. info.xml or native.jpg and filename is identifier
  string suffix;
  string filename, params, quality, format;
  int reqRegionX, reqRegionY, reqRegionWidth, reqRegionHeight, reqSizeWidth, reqSizeHeight, qualityNum;
  double rotation;
  //variables to store info about requested image mainly for info requests
  unsigned int width, height, numResolutions, tw, th;

  //number of http status code if error during parsing occured
  int errorNo = 0;
  //message of the error that occured during parsing
  string errorMsg, errorParam;

  int lastSlashPos = decodedArgument.find_last_of("/");

  //check if there is slash in argument and if it is not last / first character, extract identifier and suffix
  if( lastSlashPos < decodedArgument.length() && lastSlashPos > 0){
    suffix = decodedArgument.substr( lastSlashPos+1, string::npos );
    if( suffix.substr(0,4) == "info" ){
      filename = decodedArgument.substr(0,lastSlashPos);
    }
    else{
      int positionTmp = lastSlashPos;
      for( int i = 0; i < 3; i++) {
        positionTmp = decodedArgument.substr(0,positionTmp).find_last_of("/");
      }
      if(positionTmp > 0){
        filename = decodedArgument.substr(0,positionTmp);
        params = decodedArgument.substr(positionTmp + 1, string::npos);
      }
      else{
        errorNo = 400; //BAD REQUEST
        errorParam = "unknown";
        errorMsg = "Not enough parameters. Syntax of info request is filename/info.xml, filename/info.json or "
          "filename/info.json?callback=nameOfCallbackFunction. "
          "Syntax of image request is {identifier}/{region}/{size}/{rotation}/{quality}{.format} "
          "You have entered: " + decodedArgument;
      }
    }
  }
  else{
    errorNo = 400; //BAD REQUEST
    errorParam = "unknown";
    errorMsg = "Not enough parameters. Syntax of info request is filename/info.xml, filename/info.json or "
      "filename/info.json?callback=nameOfCallbackFunction. "
      "Syntax of image request is {identifier}/{region}/{size}/{rotation}/{quality}{.format} "
      "You have entered: " + decodedArgument;;
  }

  if( !errorNo ){
    //Check whether requested image exist
    try {
      FIF fif;
      fif.run( session, filename );
    }
    //catch any exception thrown in FIF and write error
    catch(const string& error){
      errorNo = 404; //page not found
      errorParam = "identifier";
      errorMsg =  "Requested file " + filename + " does not exist or is not supported. " + error;
    }
  }

  if( !errorNo ){
    // Load image info
    (*session->image)->loadImageInfo( session->view->xangle, session->view->yangle );
    //SOLVE IDENTIFIER PARAMETER - throw away url encoding
    filename = (*session->image)->getImagePath();

    // Get the information about image, that can be shown in info.xml or json
    width = (*session->image)->getImageWidth();
    height = (*session->image)->getImageHeight();
    tw = (*session->image)->getTileWidth();
    th = (*session->image)->getTileHeight();
    numResolutions = (*session->image)->getNumResolutions();
    if(width <= 1 || height <= 1){
      errorNo = 415; // invalid media
      errorParam = "identifier";
      errorMsg = "Requested file "+ filename +" is probably corrupted. Width or height of loaded image is 1px. You have entered: " + decodedArgument;
    }
  }


  //PARSE INPUT PARAMETERS

  //for info.xml or info.json, we just check if no argument between filename and suffix exists
  if( !errorNo && (suffix.substr(0,4) == "info")){
    if( !( suffix == "info.xml" || suffix == "info.json" || suffix.substr(0,19) == "info.json?callback=" )){
      errorNo = 400; // bad request
      errorParam = "unknown";
      errorMsg = "Wrong info request. Syntax is filename/info.xml, filename/info.json or "
        "filename/info.json?callback=nameOfCallbackFunction. You have entered: " + decodedArgument;
    }
  }

  //parse image request - any other than info requests are considered image requests
  else if( !errorNo ){
    Tokenizer izer( params, "/" );
    int numOfTokens = 0;
    //conversionChecker to detect failure of conversion from string to int or double
    char * conversionChecker;

    //SOLVE REGION PARAMETER (full; x,y,w,h; pct:x,y,w,h)
    if( !errorNo && izer.hasMoreTokens() ) {
      string regionString = izer.nextToken();
      transform( regionString.begin(), regionString.end(), regionString.begin(), ::tolower );

      if (regionString == "full"){
        reqRegionX = 0;
        reqRegionY = 0;
        reqRegionWidth = width;
        reqRegionHeight = height;
      }

      else{
        //if pct: is found, we will remember it, cut out that part of string and continue as without pct
        bool isPCT = false;
        if (regionString.substr(0,4) == "pct:"){
          isPCT = true;
          regionString = regionString.substr(4,string::npos);
        }

        // we will tokenize region (x,y,width,height)
        Tokenizer regionIzer(regionString, ",");
        int numOfSubtokens = 0;

        // X coordinate
        if( regionIzer.hasMoreTokens() ) {
          string reqRegionTemp = regionIzer.nextToken();
          if( isPCT ) {
            reqRegionX = (int) round( (width * strtod(reqRegionTemp.c_str(), &conversionChecker)) / (double) 100 );
          }
          else {
            reqRegionX = strtol(reqRegionTemp.c_str(), &conversionChecker, 10);
          }
          //check if inserted value is valid
          if (conversionChecker == reqRegionTemp || *conversionChecker != NULL
            || reqRegionX < 0 || reqRegionX > width - 1){
              errorNo = 400; //bad request
              errorParam = "region";
              errorMsg = "Region X coordinate is wrong: " + reqRegionTemp;
          }
          numOfSubtokens++;
        }

        // Y coordinate
        if( !errorNo && regionIzer.hasMoreTokens() ) {
          string reqRegionTemp = regionIzer.nextToken();
          if( isPCT ) {
            reqRegionY = (int) round( (height * strtod(reqRegionTemp.c_str(), &conversionChecker)) / (double) 100 );
          }
          else {
            reqRegionY = strtol(reqRegionTemp.c_str(), &conversionChecker, 10);
          }

          if (conversionChecker == reqRegionTemp || *conversionChecker != NULL
            || reqRegionY < 0 || reqRegionY > height - 1){
              errorNo = 400; //bad request
              errorParam = "region";
              errorMsg = "Region Y coordinate is wrong: " + reqRegionTemp;
          }
          numOfSubtokens++;
        }

        // Width of region
        if( !errorNo && regionIzer.hasMoreTokens() ) {
          string reqRegionTemp = regionIzer.nextToken();
          if( isPCT ) {
            reqRegionWidth = (int) round( (width * strtod(reqRegionTemp.c_str(), &conversionChecker)) / (double) 100 );
          }
          else {
            reqRegionWidth = strtol(reqRegionTemp.c_str(), &conversionChecker, 10);
          }
          if(reqRegionWidth > width - reqRegionX) reqRegionWidth = width - reqRegionX;

          if (conversionChecker == reqRegionTemp || *conversionChecker != NULL
            || reqRegionWidth <= 0){
              errorNo = 400; //bad request
              errorParam = "region";
              errorMsg = "Region WIDTH coordinate is wrong: " + reqRegionTemp;
          }
          numOfSubtokens++;
        }

        //Height of region
        if( !errorNo && regionIzer.hasMoreTokens() ) {
          string reqRegionTemp = regionIzer.nextToken();
          if( isPCT ) {
            reqRegionHeight = (int) round( (height * strtod(reqRegionTemp.c_str(), &conversionChecker)) / (double) 100 );
          }
          else {
            reqRegionHeight = strtol(reqRegionTemp.c_str(), &conversionChecker, 10);
          }
          if(reqRegionHeight > height- reqRegionY) reqRegionHeight = height - reqRegionY;

          if (conversionChecker == reqRegionTemp || *conversionChecker != NULL
            || reqRegionHeight <= 0){
              errorNo = 400; //bad request
              errorParam = "region";
              errorMsg = "Region HEIGHT coordinate is wrong: " + reqRegionTemp;
          }
          numOfSubtokens++;
        }
        //more region tokens
        if( !errorNo && regionIzer.hasMoreTokens() ){
          errorNo = 400; //bad request
          errorParam = "region";
          errorMsg = "Region has more parameters: " + regionString;
        }
        //less region tokens
        if( !errorNo && numOfSubtokens < 4 ){
          errorNo = 400; //bad request
          errorParam = "region";
          errorMsg = "Region has less parameters: " + regionString;
        }
      }//end of else - end of parsing x,y,w,h

      if( errorNo ){
        errorMsg += " Region format is: full, x,y,width,height or pct:x,y,width,height. Your full request is: "
          + decodedArgument;
      }

      numOfTokens++;
      if ( !errorNo && session->loglevel > 3){
        *(session->logfile) << "IIIF :: requested region of image is x:" << reqRegionX << ", y:" << reqRegionY
          << ", width:" << reqRegionWidth << ", height:" << reqRegionHeight << endl;
      }

    }//end of region parameter

    //SOLVE SIZE PARAMETER
    if( !errorNo && izer.hasMoreTokens() ) {
      int sizeLimit = Environment::getMaxCVT();
      double aspectRatio = reqRegionWidth / (double) reqRegionHeight; //w = h * ar, h = w / ar
      string sizeString = izer.nextToken();
      transform( sizeString.begin(), sizeString.end(), sizeString.begin(), ::tolower );

      //full request
      if( sizeString == "full" ){
        reqSizeWidth = reqRegionWidth;
        reqSizeHeight = reqRegionHeight;
      }

      //pct:n request
      else if( sizeString.substr(0,4) == "pct:" || sizeString.substr(0,5) == "!pct:"){
        int pctPos = sizeString.find_first_of(":") + 1;
        double sizePercentage = strtod( sizeString.substr(pctPos,string::npos).c_str(), &conversionChecker );
        if ( *conversionChecker != NULL || sizePercentage <= 0 || sizePercentage > 400 ) {
          errorNo = 400; //bad request
          errorParam = "size";
          errorMsg = "Size percentage must be number between 1 and 400, you have entered: "
            + sizeString.substr(pctPos,string::npos);
        }
        else{
          reqSizeWidth = (int) round((reqRegionWidth * sizePercentage)/(double)100);
          reqSizeHeight = (int) round((reqRegionHeight * sizePercentage)/(double)100);
          //put hard limit for pct command, because it can round very little values to zero and send corrupted
          if(reqSizeWidth < 1) reqSizeWidth = 1;
          if(reqSizeHeight < 1) reqSizeHeight = 1;
        }
      }

      // w,h   w,    ,h    !w,h requests
      else{
        //indicates that before width is excl. mark
        bool isExclamationMark = false;
        //indicates that width should be counted from height
        bool isBlankWidth = false;

        //!w,h request - remove !, remember it and continue as if w,h request
        if( sizeString.substr(0,1) == "!" ) {
          isExclamationMark = true;
          sizeString = sizeString.substr(1, string::npos);
        }

        //tokenizer will not return empty string, so we won't use it for size parsing
        int commaPosition = sizeString.find_first_of(",");
        int commaPosition2 = sizeString.substr(commaPosition+1, string::npos).find_first_of(",");
        if( commaPosition < 0 ||  commaPosition2 >= 0){
          errorNo = 400; //bad request
          errorParam = "size";
          errorMsg = "Not right amount of size parameters. You must insert {width,height} or {width,} or {,height}. "
            "You have entered: " + sizeString;
        }

        string sizeToken;

        //width
        if (!errorNo){
          sizeToken =  sizeString.substr(0,commaPosition);

          //width is not set
          if( sizeToken.empty() ){
            if( isExclamationMark ){
              errorNo = 400; //bad request
              errorParam = "size";
              errorMsg = "You requested image that fits into specific width and height, but did't specified width."
                "You have entered: " + sizeString;
            }
            isBlankWidth = true;
          }

          //width is set
          else {
            reqSizeWidth = strtol(sizeToken.c_str(), &conversionChecker, 10);
            if( *conversionChecker != NULL || reqSizeWidth <= 0 || reqSizeWidth > width*4){
              errorNo = 400; //bad request
              errorParam = "size";
              errorMsg = "Size width must be positive integer between 1 and 4x width of the original image."
                "You have entered: " + sizeToken;
            }
          }
        }

        //height
        if( !errorNo ){
          sizeToken = sizeString.substr(commaPosition+1, string::npos);

          //height is not set
          if( sizeToken.empty() ){
            if (isExclamationMark){
              errorNo = 400; //bad request
              errorParam = "size";
              errorMsg = "You requested image that fits into specific width and height, but did't specified height."
                "You have entered: " + sizeString;
            }
            if( isBlankWidth ){
              errorNo = 400; //bad request
              errorParam = "size";
              errorMsg = "You must enter at least one of width or height (,height or width,)."
                "You have entered: " + sizeString;
            }
            else {
              reqSizeHeight = (int) round(reqSizeWidth / aspectRatio); //w = h * ar, h = w / ar, ar = w / h
            }
          }

          //height is set
          else {
            reqSizeHeight = strtol(sizeToken.c_str(), &conversionChecker, 10);
            if( *conversionChecker != NULL || reqSizeHeight <= 0 || reqSizeHeight > height*4){
              errorNo = 400; //bad request
              errorParam = "size";
              errorMsg = "Size height must be positive integer between 1 and 4x height of the original image."
                "You have entered: " + sizeToken;
            }
            if( isBlankWidth ) {
              reqSizeWidth = (int) round(reqSizeHeight * aspectRatio); //w = h * ar, h = w / ar, ar = w / h
            }
          }
        }

        //  !w,h - modify higher value if it should keep aspect ratio and fit into limits
        if( isExclamationMark ){
          if ( (reqSizeHeight * aspectRatio) > reqSizeWidth ){
            reqSizeHeight = (int) round (reqSizeWidth / aspectRatio);
          }
          if ( (reqSizeWidth / aspectRatio) > reqSizeHeight ){
            reqSizeWidth = (int) round (reqSizeHeight * aspectRatio);
          }
        }
      }

      // limit requested size according to MAX_CVT
      if( sizeLimit != -1 && (reqSizeWidth > sizeLimit || reqSizeHeight > sizeLimit) ){
        // this is aspect ratio of requested resolution (may be different than one of the requested region)
        double reqAspectRatio = reqSizeWidth / (double) reqSizeHeight;
        if( reqSizeWidth > sizeLimit ){
          reqSizeWidth = sizeLimit;
          reqSizeHeight = (int) round(reqSizeWidth / reqAspectRatio);
        }
        if( reqSizeHeight > sizeLimit ){
          reqSizeHeight = sizeLimit;
          reqSizeWidth = (int) round(reqSizeHeight * reqAspectRatio);
        }
      }

      if( errorNo ){
        errorMsg += " Size format is: full or width,height or width, or ,heigh or pct:x or !width,height. "
          "Your full request is: " + decodedArgument;
      }

      numOfTokens++;
      if( !errorNo && session->loglevel >= 3 ){
        *(session->logfile) << "IIIF :: requested size of image is width: " << reqSizeWidth 
          << ", height: " << reqSizeHeight << endl;
      }
    }

    //SOLVE ROTATION PARAMETER
    if( !errorNo && izer.hasMoreTokens() ){
      string rotationString = izer.nextToken();
      rotation = strtod(rotationString.c_str(), &conversionChecker);

      //check if conversion was successful
      if( conversionChecker == rotationString || *conversionChecker != NULL
        || rotation < 0 || rotation > 360){
          errorNo = 400;
          errorParam = "rotation";
          errorMsg = "Rotation parameter must be decimal number between 0 and 360. "
            "You have entered: " + rotationString;
      }

      //check if converted value is supported
      if(!( rotation == 0 || rotation == 90 || rotation == 180 ||
        rotation == 270 || rotation == 360 )){
          errorNo = 501;
          errorParam = "rotation";
          errorMsg = "Currently implemented rotation angles are 0, 90, 180 and 270 degrees. "
            "You have entered: " + rotationString;
      }
      numOfTokens++;

      if( !errorNo && session->loglevel >= 3 ){
        *(session->logfile) << "IIIF :: requested rotation of image is: " << rotation << endl;
      }
    }

    //SOLVE QUALITY AND FORMAT PARAMETERS
    if( !errorNo && izer.hasMoreTokens() ){
      string quality = izer.nextToken();
      transform( quality.begin(), quality.end(), quality.begin(), ::tolower );
      int positionDot = quality.find_last_of(".");

      //if dot is not present, we use default and currently only supported format - jpg
      if (positionDot == string::npos){
        format = "jpg";
      }
      else {
        format = quality.substr(positionDot+1,string::npos);
        quality = quality.substr(0,positionDot);
      }

      //quality
      qualityNum = strtol(quality.c_str(), &conversionChecker, 10);

      if( quality == "native" || quality == "color" || quality == "grey" || quality == "bitonal" ||
        (qualityNum >= 0 && qualityNum <= 100 && *conversionChecker == NULL && conversionChecker != quality) ) {
          // if one of unsupported formats
          if ( quality == "color" && quality == "grey" || quality == "bitonal" ){
            errorNo = 501;
            errorParam = "quality";
            errorMsg = "Currently implemented quality parameters are native or number "
              "between 0 and 100, that implies quality of jpg (0 means best compression, 100 means best quality)";
          }
          // if value for jpeg quality is 0, set it to 1 (lowest reasonable value)
          else if ( quality != "native" && qualityNum == 0 ){
            qualityNum = 1;
          }
      }
      else {
        errorNo = 400;
        errorParam = "quality";
        errorMsg = "Quality parameter must be one of: native, color, grey, bitonal or number between 0 and 100, "
          "that represents quality of jpg (0 means best compression, 100 means best quality)."
          " You have entered: " + quality;
      }

      // if not jpg format, write appropriate error
      if( !errorNo ){
        if( format == "jpg" || format == "tif" || format == "png" || format == "gif"
          || format == "jp2" || format == "pdf" ) {
            if( format != "jpg" ){
              errorNo = 415;
              errorParam = "format";
              errorMsg = "Currently, jpg is the only implemented format.";
            }
        }
        else {
          errorNo = 400;
          errorParam = "format";
          errorMsg = "Format must be one of: jpg, tif, png, gif, jp2 or pdf."
            " You have entered: " + format;
        }
      }
      numOfTokens++;

      if( errorNo ){
        errorMsg += " Your full request is: " + decodedArgument;
      }
      if( !errorNo && session->loglevel >= 3 ){
        *(session->logfile) << "IIIF :: requested quality of image is: " << quality
          << ", requested format is: " << format << endl;
      }
    }

    //TOO MUCH PARAMETERS, tell it to user and show him his request
    if( !errorNo && izer.hasMoreTokens() ){
      errorNo = 400;
      errorParam = "unknown";
      errorMsg = "Inserted query has more parameters. "
        "Syntax should be {identifier}/{region}/{size}/{rotation}/{quality}{.format} "
        "You have entered: " + decodedArgument;
    }
    //NOT ENOUGH PARAMETERS
    if( !errorNo && numOfTokens < 4 ){
      errorNo = 400;
      errorParam = "unknown";
      errorMsg = "Inserted query has not enough parameters. "
        "Syntax should be {identifier}/{region}/{size}/{rotation}/{quality}{.format} "
        "You have entered: " + decodedArgument;
    }
  }
  //END OF PARSING OF INPUT PARAMETERS

  //WRITE FIRST PARSING ERROR
  if( errorNo ){
    string statusMsg;
    if( errorNo == 400 ){
      statusMsg = "Bad Request";
    }
    else if( errorNo == 404 ){
      statusMsg = "Not Found";
    }
    else if( errorNo == 415 ){
      statusMsg = "Invalid Media";
    }
    else{
      statusMsg = "Not implemented";//501
    }
    char str[2048];
    string errorXmlRespond = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<error xmlns=\"http://library.stanford.edu/iiif/image-api/ns/\">\n"
      "<parameter>" + errorParam + "</parameter>\n"
      "<text>" + errorMsg + "</text>\n"
      "</error>";
    snprintf( str, 2048,
      "Server: iipsrv/%s\r\n"
      "Cache-Control: no-cache\r\n"
      "Content-Type: text/xml\r\n"//specification specifically requires text/xml (not application/xml)
      "Status: %d %s\r\n"
      "\r\n"
      "%s",
      VERSION, errorNo, statusMsg.c_str(), errorXmlRespond.c_str() );
    session->out->printf((const char*) str);
    session->out->flush();

    *(session->logfile) << "IIIF :: Parsing error occured. " << errorMsg
      << " Entered argument was " << decodedArgument << endl;
    throw errorNo;
  }

  //write info about request to log
  if( suffix == "info.xml" || suffix == "info.json" ){
    if( session->loglevel >= 3 ){
      *(session->logfile) << "IIIF :: " << suffix << " request for " << (*session->image)->getImagePath() << endl;
    }
  }
  else {
    if( session->loglevel >= 3 ){
      *(session->logfile) << "IIIF :: image request for " << (*session->image)->getImagePath()
        << " with arguments: region - " << reqRegionX << "," << reqRegionY << ","
        << reqRegionWidth << "," << reqRegionHeight << ", size - " << reqSizeWidth << "," << reqSizeHeight
        << ", rotation - " << rotation << ", quality - " << quality << ", format - " << format << endl;
    }
  }

  // INFO.XML OUPUT
  if( suffix == "info.xml" ){
    // XML encoding of filename
    string escapedFilename = "";
    for (int i = 0; i < filename.length(); i++){
      char c = filename[i];
      switch(c){
        case '&':
          escapedFilename += "&#38;";
          break;
        case '<':
          escapedFilename += "&#60;";
          break;
        case '>':
          escapedFilename += "&#62;";
          break;
        case '"':
          escapedFilename += "&#34;";
          break;
        case '\'':
          escapedFilename += "&#39;";
          break;
        default:
          escapedFilename += c;
      }
    }

    std::stringstream xmlStringStream;
    xmlStringStream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;
    xmlStringStream << "<info xmlns=\"http://library.stanford.edu/iiif/image-api/ns/\">" << endl;
    xmlStringStream << "<identifier>" << escapedFilename << "</identifier>" << endl;
    xmlStringStream << "<width>" << width << "</width>" << endl;
    xmlStringStream << "<height>" << height << "</height>" << endl;
    xmlStringStream << "<scale_factors>" << endl;
    //the scale_factors element expresses a list of resolution scaling factors.
    for (int i=0; i < numResolutions; i++){
      xmlStringStream << "<scale_factor>" << std::pow(2.0,i) << "</scale_factor>" << endl;
    }
    xmlStringStream << "</scale_factors>" << endl;
    xmlStringStream << "<tile_width>" << tw << "</tile_width>" << endl;
    xmlStringStream << "<tile_height>" << th << "</tile_height>" << endl;
    xmlStringStream << "<formats>" << endl;
    xmlStringStream << "<format>jpg</format>" << endl;
    xmlStringStream << "</formats>" << endl;
    xmlStringStream << "<qualities>" << endl;
    xmlStringStream << "<quality>native</quality>" << endl;
    xmlStringStream << "</qualities>" << endl;
    xmlStringStream << "<profile>http://library.stanford.edu/iiif/image-api/compliance.html#level1</profile>" << endl;
    xmlStringStream << "</info>";

    char str[2048];
    snprintf( str, 2048,
      "Server: iipsrv/%s\r\n"
      "Content-Type: application/xml\r\n"
      "Cache-Control: max-age=%d\r\n"
      "Last-Modified: %s\r\n"
      "\r\n"
      "%s",
      VERSION, MAX_AGE,(*session->image)->getTimestamp().c_str(), xmlStringStream.str().c_str() );

    session->out->printf( (const char*) str );
    session->response->setImageSent();
    return;
  }
  // INFO.JSON OUTPUT
  else if( suffix.substr(0,9) == "info.json" ){
    // JSON encoding of filename
    string escapedFilename = "";
    for (int i = 0; i < filename.length(); i++){
      char c = filename[i];
      switch(c){
        case '\\':
          escapedFilename += "\\\\";
          break;
        case '"':
          escapedFilename += "\\\"";
          break;
        default:
          escapedFilename += c;
      }
    }

    std::stringstream jsonStringStream;
    if( suffix.length() > 19 ){
      jsonStringStream << suffix.substr(19,string::npos) << "(";
    }
    jsonStringStream << "{" << endl;
	jsonStringStream << "\"@context\" : \"http://library.stanford.edu/iiif/image-api/1.1/context.json\"," << endl;
	string fabricUrl = Environment::getFabricUrl();
	if(!fabricUrl.empty()){
	  jsonStringStream << "\"@id\" : \"" << fabricUrl << escapedFilename << "\"," << endl;
	}
    jsonStringStream << "\"width\" : " << width << "," << endl;
    jsonStringStream << "\"height\" : " << height << "," << endl;
    //sf 1 is always present, it is original image
    jsonStringStream << "\"scale_factors\" : [ 1";
    for (int i=1; i < numResolutions; i++){
      jsonStringStream << ", " << std::pow(2.0,i);
    }
    jsonStringStream << " ]," << endl;
    jsonStringStream << "\"tile_width\" : " << tw << "," << endl;
    jsonStringStream << "\"tile_height\" : " << th << "," << endl;
    jsonStringStream << "\"formats\" : [ \"jpg\" ]," << endl;
    jsonStringStream << "\"qualities\" : [ \"native\" ]," << endl;
    jsonStringStream << "\"profile\" : \"http://library.stanford.edu/iiif/image-api/compliance.html#level1\"" << endl; 
    jsonStringStream<< "}";
    if( suffix.length() > 19 ){
      jsonStringStream << ");";
    }

    string jsonMime;
    if( suffix.length() > 19 )  jsonMime = "javascript";
    else jsonMime = "json";

    char str[2048];
    snprintf( str, 2048,
      "Server: iipsrv/%s\r\n"
      "Content-Type: application/%s\r\n"
      "Cache-Control: max-age=%d\r\n"
      "Last-Modified: %s\r\n"
      "\r\n"
      "%s",
      VERSION, jsonMime.c_str(), MAX_AGE,(*session->image)->getTimestamp().c_str(), jsonStringStream.str().c_str() );
    session->out->printf( (const char*) str );
    session->response->setImageSent();
    return;
  }

  // IMAGE REQUEST (all requests other than info requests are considered image requests)
  else {

    //magic - adjusting region to fit rounding of IIIF although IIPImage is truncating
    //magicConstant corresponds to scale factor of requested quality layer
    double magicConstant = reqRegionWidth / (double) reqSizeWidth;
    //there will be used 1 quality layer, so scaling factor will be same for height and width, the lesser one
    if (reqRegionHeight / (double) reqSizeHeight < magicConstant) magicConstant = reqRegionHeight / (double) reqSizeHeight;
    int cropLeft = 0;
    int cropRight = 0;
    int cropTop = 0;
    int cropBottom = 0;


    //for smaller size than original
    if(magicConstant > 1){
      if(reqSizeWidth > 0 && reqRegionWidth % reqSizeWidth >= reqSizeWidth*0.5){
        reqRegionWidth += (int) round(magicConstant*0.5);
        *(session->logfile) << "IIIF :: Adjusting Region - New ReqRegWid:" << reqRegionWidth << endl;
      }

      if(round(magicConstant) > 0 && reqRegionX % (int) round(magicConstant) > 0){
        reqRegionX += (int) round(magicConstant*0.5);
        *(session->logfile) << "IIIF :: Adjusting Region - New ReqRegX:" << reqRegionX << endl;
      }

      if(reqSizeHeight > 0 && reqRegionHeight % reqSizeHeight >= reqSizeHeight*0.5){
        reqRegionHeight += (int) round(magicConstant*0.5);
        *(session->logfile) << "IIIF :: Adjusting Region - New ReqRegHei:" << reqRegionHeight << endl;
      }

      if(round(magicConstant) > 0 && reqRegionY % (int) round(magicConstant) > 0){
        reqRegionY += (int) round(magicConstant*0.5);
        *(session->logfile) << "IIIF :: Adjusting Region - New ReqRegY:" << reqRegionY << endl;
      }
    }
    //for bigger size than original
    else if(magicConstant < 1){
      //pixelRatio - ratio between original pixel and scaled pixel, unit means how many pixels are added
      int pixelWidthRatio, pixelHeightRatio, unitX, unitY;

      //for 150, 250 and 350% ask 2 pixels instead of 1 for higher precision
      if(reqSizeWidth/(double)reqRegionWidth - reqSizeWidth/reqRegionWidth > 0.3
        && reqSizeWidth/(double)reqRegionWidth  - reqSizeWidth/reqRegionWidth < 0.7){
        pixelWidthRatio = (int) round(2*(reqSizeWidth/(double)reqRegionWidth));
        unitX = 2;
      }
      else{
        pixelWidthRatio = (int) round(reqSizeWidth/(double)reqRegionWidth);
        unitX = 1;
      }
      //the same for vertical resize
      if(reqSizeHeight/(double)reqRegionHeight - reqSizeHeight/reqRegionHeight > 0.3
        && reqSizeHeight/(double)reqRegionHeight - reqSizeHeight/reqRegionHeight < 0.7){
        pixelHeightRatio = (int) round(2*(reqSizeHeight/(double)reqRegionHeight));
        unitY = 2;
      }
      else{
        pixelHeightRatio = (int) round(reqSizeHeight/(double)reqRegionHeight);
        unitY = 1;
      }

      if(reqRegionX  - unitX >= 0){
        reqRegionX -= unitX;
        reqRegionWidth += unitX;
        reqSizeWidth += pixelWidthRatio;
        cropLeft = pixelWidthRatio;
      }
      if(reqRegionY  - unitY >= 0){
        reqRegionY -= unitY;
        reqRegionHeight += unitY;
        reqSizeHeight += pixelHeightRatio;
        cropTop = pixelHeightRatio;
      }
      if(reqRegionWidth  + cropLeft + unitX <= width){
        reqRegionWidth += unitX;
        reqSizeWidth += pixelWidthRatio;
        cropRight = pixelWidthRatio;
      }
      if(reqRegionHeight  + cropTop + unitY <= height){
        reqRegionHeight += unitX;
        reqSizeHeight += pixelHeightRatio;
        cropBottom = pixelHeightRatio;
      }
    }

    session->view->setImageSize( width, height );
    session->view->setMaxResolutions( numResolutions );
    //set requested REGION limits - if requested region is out of image, it is solved in View.cc
    session->view->setViewLeft( reqRegionX / (double) width );
    session->view->setViewTop( reqRegionY / (double) height );
    session->view->setViewWidth( reqRegionWidth / (double) width );
    session->view->setViewHeight( reqRegionHeight / (double) height );
    //set requested SIZE limits
    session->view->setRequestWidth( reqSizeWidth );
    session->view->setRequestHeight( reqSizeHeight );
    //get most suitable resolution and recalculate width and height of region in this resolution
    int requested_res = session->view->getResolution();

#ifndef DEBUG

    // Define our separator depending on the OS
#ifdef WIN32
    const string separator = "\\";
#else
    const string separator = "/";
#endif


    // Get our image file name and strip of the directory path and any suffix
    filename = (*session->image)->getImagePath();
    int pos = filename.rfind(separator)+1;
    string basename = filename.substr( pos, filename.rfind(".")-pos );

    char str[2048];
    snprintf( str, 2048,
      "Server: iipsrv/%s\r\n"
      "Cache-Control: max-age=%d\r\n"
      "Last-Modified: %s\r\n"
      "Content-Type: image/jpeg\r\n"
      "Content-Disposition: inline;filename=\"%s.jpg\"\r\n"
      "\r\n",
      VERSION, MAX_AGE, (*session->image)->getTimestamp().c_str(), basename.c_str() );

    session->out->printf( (const char*) str );
#endif


    // *** GET REQUESTED REGION ***

    // Get our requested region from our TileManager
    TileManager tilemanager( session->tileCache, *session->image, session->watermark, session->jpeg, session->logfile,
      session->loglevel );
    
    RawTile complete_image = tilemanager.getRegion( requested_res, session->view->xangle, session->view->yangle,
      session->view->getLayers(), session->view->getViewLeft(), session->view->getViewTop(),
      session->view->getViewWidth(), session->view->getViewHeight() );

    if( session->loglevel >= 4 ){
      *(session->logfile) << "IIIF :: Requested region retrieved, requested resolution: "<< requested_res
        << ", region in this resolution X,Y,W,H: "<< session->view->getViewLeft() <<","<< session->view->getViewTop()<<","
        << session->view->getViewWidth()<<"," << session->view->getViewHeight()<< endl;
    }


    // Convert CIELAB to sRGB
    if( (*session->image)->getColourSpace() == CIELAB ){
      Timer cielab_timer;
      if( session->loglevel >= 3 ){
        *(session->logfile) << "IIIF :: Converting from CIELAB->sRGB" << endl;
        cielab_timer.start();
      }
      filter_LAB2sRGB( complete_image );
      if( session->loglevel >= 3 ){
        *(session->logfile) << "IIIF :: CIELAB->sRGB conversion in " << cielab_timer.getTime()
          << " microseconds" << endl;
      }
    }

    // Apply normalization and float conversion
    filter_normalize( complete_image, (*session->image)->max, (*session->image)->min );

    // *** RESIZE IMAGE ***

    // Resize our image as requested. Use the interpolation method requested in the server configuration - bilinear default
    if( (reqSizeWidth != complete_image.width) || (reqSizeHeight != complete_image.height) ){
      if( session->loglevel >= 5 ){
        *(session->logfile) << "Resizing is required." << endl;
      }
      
      Timer interpolation_timer;
      string interpolation_type;
      if( session->loglevel >= 5 ){
        interpolation_timer.start();
      }
      unsigned int interpolation = Environment::getInterpolation();
      switch( interpolation ){
      case 0:
        interpolation_type = "nearest neighbour";
        filter_interpolate_nearestneighbour( complete_image, reqSizeWidth, reqSizeHeight );
        break;
      default:
        interpolation_type = "bilinear";
        filter_interpolate_bilinear( complete_image, reqSizeWidth, reqSizeHeight );
        break;
      }

      if( session->loglevel >= 5 ){
        *(session->logfile) << "IIIF :: Resizing using " << interpolation_type << " interpolation in "
          << interpolation_timer.getTime() << " microseconds, new width: "<< reqSizeWidth
          << ", new height:" << reqSizeHeight << endl;
      }
    }//END OF RESIZING

    // *** CROP IMAGE ***
    if(cropBottom || cropLeft || cropRight || cropTop){
      Timer crop_timer;
      if( session->loglevel >= 5 ){
        crop_timer.start();
      }

      filter_crop( complete_image, cropLeft, cropTop, cropRight, cropBottom );
      reqSizeWidth = reqSizeWidth - cropLeft - cropRight;
      reqSizeHeight = reqSizeHeight - cropTop - cropBottom;

      if( session->loglevel >= 5 ){
        *(session->logfile) << "IIIF :: Cropping in "
          << crop_timer.getTime() << " microseconds, cropped by: "<< cropLeft
          << "," << cropTop << "," << cropRight << "," << cropBottom <<endl;
      }
    }//END OF CROPPING

    // Convert from float to 8bit RGB
    filter_contrast( complete_image, 1.0f );

    // *** ROTATE IMAGE ***
    if((int)rotation % 360 != 0){
      Timer rotationTimer;
      if( session->loglevel >= 4 ){
        rotationTimer.start();
      }

      filter_rotate(complete_image, rotation);

      //switch required width and height
      if((int) rotation % 180 == 90){
        int tmp = reqSizeHeight;
        reqSizeHeight = reqSizeWidth;
        reqSizeWidth = tmp;
      }

      if( session->loglevel >= 4 ){
        *(session->logfile) << "IIIF :: Rotating image by " << (int) rotation % 360 << " degrees in "
          << rotationTimer.getTime() << " microseconds" << endl;
      }
    }//END OF ROTATION
    

    // *** SEND RESULT ***

    //set quality if specified in request
    if( qualityNum ){
      session->jpeg->setQuality(qualityNum);
    }
    // Initialise our JPEG compression object
    session->jpeg->InitCompression( complete_image, reqSizeHeight );
    int len = session->jpeg->getHeaderSize();

    if( session->out->putStr( (const char*) session->jpeg->getHeader(), len ) != len ){
      if( session->loglevel >= 1 ){
        *(session->logfile) << "IIIF :: Error writing jpeg header" << endl;
      }
    }

    // Flush our block of data
    if( session->out->flush() == -1 ) {
      if( session->loglevel >= 1 ){
        *(session->logfile) << "IIIF :: Error flushing jpeg data" << endl;
      }
    }

    // Send out the data per strip of fixed height.
    // Allocate enough memory for this plus an extra 16k for instances where compressed
    // data is greater than uncompressed
    unsigned int strip_height = 128;
    unsigned char* output = new unsigned char[reqSizeWidth*complete_image.channels*strip_height+16536];
    int strips = (reqSizeHeight/strip_height) + (reqSizeHeight % strip_height == 0 ? 0 : 1);

    for( int n=0; n<strips; n++ ){

      // Get the starting index for this strip of data
      unsigned char* input = &((unsigned char*)complete_image.data)[n*strip_height*reqSizeWidth*complete_image.channels];

      // The last strip may have a different height
      if( (n==strips-1) && (reqSizeHeight%strip_height!=0) ) strip_height = reqSizeHeight % strip_height;

      if( session->loglevel >= 3 ){
        *(session->logfile) << "IIIF :: About to JPEG compress strip with height " << strip_height << endl;
      }

      // Compress the strip
      len = session->jpeg->CompressStrip( input, output, strip_height );

      if( session->loglevel >= 3 ){
        *(session->logfile) << "IIIF :: Compressed data strip length is " << len << endl;
      }

      // Send this strip out to the client
      if( len != session->out->putStr( (const char*) output, len ) ){
        if( session->loglevel >= 1 ){
          *(session->logfile) << "IIIF :: Error writing jpeg strip data: " << len << endl;
        }
      }

      // Flush our block of data
      if( session->out->flush() == -1 ) {
        if( session->loglevel >= 1 ){
          *(session->logfile) << "IIIF :: Error flushing jpeg data" << endl;
        }
      }

    }//END OF FOR

    // Finish off the image compression
    len = session->jpeg->Finish( output );

    if( session->out->putStr( (const char*) output, len ) != len ){
      if( session->loglevel >= 1 ){
        *(session->logfile) << "IIIF :: Error writing jpeg EOI markers" << endl;
      }
    }

    delete[] output;

    if( session->out->flush()  == -1 ) {
      if( session->loglevel >= 1 ){
        *(session->logfile) << "IIIF :: Error flushing jpeg tile" << endl;
      }
    }

    // Inform our response object that we have sent something to the client
    session->response->setImageSent();

  }//END OF IMAGE REQUEST

  // Total IIIF response time
  if( session->loglevel >= 2 ){
    *(session->logfile) << "IIIF :: Total command time " << command_timer.getTime() << " microseconds" << endl;
  }

}

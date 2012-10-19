/*
    IIIF Request Command Handler Class Member Function
	Author: Michal Becak
*/

#include <cmath>
#include <algorithm>
#include <sstream>
#include "Task.h"
#include "Transforms.h"
#include "Tokenizer.h"
#include "Environment.h"

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

  //variables to store information from url request
  //suffix is last parameter eg. info.xml or native.jpg and filename is identifier
  string suffix;
  string filename, quality, format;
  int reqRegionX, reqRegionY, reqRegionWidth, reqRegionHeight, reqSizeWidth, reqSizeHeight, qualityNum;
  double rotation;
  //variables to store info about requested image mainly for info requests
  unsigned int width, height, numResolutions, tw, th;

  //number of http status code if error during parsing occured
  int errorNo = 0;
  //message of the error that occured during parsing
  string errorMsg;

  int firstSlashPos = argument.find_first_of("/");
  int lastSlashPos = argument.find_last_of("/");

  //check if there is slash in argument and if it is not last / first character, extract identifier and suffix
  if( lastSlashPos < argument.length() && firstSlashPos < argument.length()
	  && lastSlashPos > 0 && firstSlashPos > 0 ){
		  suffix = argument.substr( lastSlashPos+1, string::npos );
		  filename = argument.substr( 0, firstSlashPos );
  }
  else{
	errorNo = 400; //BAD REQUEST
	errorMsg = "Not enough parameters. Slash is not present or is first or last character.";
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
		  errorMsg =  "Requested file " + filename + " does not exist or is not supported. "
			  + error;
	  }
  }

  if( !errorNo ){
	  // Load image info
	  (*session->image)->loadImageInfo( session->view->xangle, session->view->yangle );

	  // Get the information about image, that can be shown in info.xml or json
	  width = (*session->image)->getImageWidth();
	  height = (*session->image)->getImageHeight();
	  tw = (*session->image)->getTileWidth();
	  th = (*session->image)->getTileHeight();
	  numResolutions = (*session->image)->getNumResolutions();
  }


  //PARSE INPUT PARAMETERS

  //for info.xml or info.json, we just check if no argument between filename and suffix exists
  if( !errorNo && (suffix == "info.xml" || suffix == "info.json")){
	if( firstSlashPos != lastSlashPos ){
		errorNo = 400; // bad request
		errorMsg = "Too much parameters in info request. Syntax is filename/info.xml or filename/info.json.";
	}
  }

  //parse image request - any other than info requests are considered image requests
  else if( !errorNo ){
	Tokenizer izer( argument, "/" );
	int numOfTokens = 0;
	//conversionChecker to detect failure of conversion from string to int or double
	char * conversionChecker;

	//SOLVE IDENTIFIER PARAMETER
	if( izer.hasMoreTokens() ) {
		//skip first token (we don't need url encoded filename) and assign image path to filename
		//filename parsing was already solved in FIF request
		izer.nextToken();
		filename = (*session->image)->getImagePath();
		numOfTokens++;
	}

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
				regionString = regionString.substr(3,string::npos);
			}

			// we will tokenize region (x,y,width,height)
			Tokenizer regionIzer(regionString, ",");
			int numOfSubtokens = 0;

			// X coordinate
			if( regionIzer.hasMoreTokens() ) {
				string reqRegionTemp = regionIzer.nextToken();
				if( isPCT ) {
					reqRegionX = round( (width * strtod(reqRegionTemp.c_str(), &conversionChecker)) / (double) 100 );
				}
				else {
					reqRegionX = strtol(reqRegionTemp.c_str(), &conversionChecker, 10);
				}
				//check if inserted value is valid
				if (conversionChecker == reqRegionTemp || *conversionChecker != NULL
					|| reqRegionX < 0 || reqRegionX > width - 1){
					errorNo = 400; //bad request
					errorMsg = "Region X coordinate is wrong: " + reqRegionTemp;
				}
				numOfSubtokens++;
			}

			// Y coordinate
			if( !errorNo && regionIzer.hasMoreTokens() ) {
				string reqRegionTemp = regionIzer.nextToken();
				if( isPCT ) {
					reqRegionY = round( (height * strtod(reqRegionTemp.c_str(), &conversionChecker)) / (double) 100 );
				}
				else {
					reqRegionY = strtol(reqRegionTemp.c_str(), &conversionChecker, 10);
				}

				if (conversionChecker == reqRegionTemp || *conversionChecker != NULL
					|| reqRegionY < 0 || reqRegionY > height - 1){
					errorNo = 400; //bad request
					errorMsg = "Region Y coordinate is wrong: " + reqRegionTemp;
				}
				numOfSubtokens++;
			}

			// Width of region
			if( !errorNo && regionIzer.hasMoreTokens() ) {
				string reqRegionTemp = regionIzer.nextToken();
				if( isPCT ) {
					reqRegionWidth = round( (width * strtod(reqRegionTemp.c_str(), &conversionChecker)) / (double) 100 );
				}
				else {
					reqRegionWidth = strtol(reqRegionTemp.c_str(), &conversionChecker, 10);
				}

				if (conversionChecker == reqRegionTemp || *conversionChecker != NULL
					|| reqRegionWidth <= 0 || reqRegionWidth > width){
					errorNo = 400; //bad request
					errorMsg = "Region WIDTH coordinate is wrong: " + reqRegionTemp;
				}
				numOfSubtokens++;
			}

			//Height of region
			if( !errorNo && regionIzer.hasMoreTokens() ) {
				string reqRegionTemp = regionIzer.nextToken();
				if( isPCT ) {
					reqRegionHeight = round( (height * strtod(reqRegionTemp.c_str(), &conversionChecker)) / (double) 100 );
				}
				else {
					reqRegionHeight = strtol(reqRegionTemp.c_str(), &conversionChecker, 10);
				}

				if (conversionChecker == reqRegionTemp || *conversionChecker != NULL
					|| reqRegionHeight <= 0 || reqRegionHeight > height){
					errorNo = 400; //bad request
					errorMsg = "Region HEIGHT coordinate is wrong: " + reqRegionTemp;
				}
				numOfSubtokens++;
			}
			//more region tokens
			if( !errorNo && regionIzer.hasMoreTokens() ){
				errorNo = 400; //bad request
				errorMsg = "Region has more parameters: " + regionString;
			}
			//less region tokens
			if( !errorNo && numOfSubtokens < 4 ){
				errorNo = 400; //bad request
				errorMsg = "Region has less parameters: " + regionString;
			}
		}//end of else - end of parsing x,y,w,h

		numOfTokens++;
		if ( !errorNo && session->loglevel > 3){
			*(session->logfile) << "IIIF :: requested region of image is x:" << reqRegionX << ", y:" << reqRegionY
				<< ", width:" << reqRegionWidth << ", height:" << reqRegionHeight << endl;
		}

	}//end of region parameter

	//SOLVE SIZE PARAMETER
	if( !errorNo && izer.hasMoreTokens() ) {
		double aspectRatio = width / (double) height; //w = h * ar, h = w / ar
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
			if ( *conversionChecker != NULL
				 || sizePercentage <= 0 || sizePercentage > 400 ) {
				errorNo = 400; //bad request
				errorMsg = "Size percentage must be number between 1 and 400, you have entered: "
									+ sizeString.substr(pctPos,string::npos);
			}
			else{
				reqSizeWidth = round((reqRegionWidth * sizePercentage)/(double)100);
				reqSizeHeight = round((reqRegionHeight * sizePercentage)/(double)100);
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
						errorMsg = "You requested image that fits into specific width and height, but did't specified height."
						"You have entered: " + sizeString;
					}
					if( isBlankWidth ){
						errorNo = 400; //bad request
						errorMsg = "You must enter at least one of width or height (,height or width,)."
						"You have entered: " + sizeString;
					}
					else {
						reqSizeHeight = round(reqSizeWidth / aspectRatio); //w = h * ar, h = w / ar, ar = w / h
					}
				}

				//height is set
				else {
					reqSizeHeight = strtol(sizeToken.c_str(), &conversionChecker, 10);
					if( *conversionChecker != NULL || reqSizeHeight <= 0 || reqSizeHeight > height*4){
						errorNo = 400; //bad request
						errorMsg = "Size height must be positive integer between 1 and 4x height of the original image."
						"You have entered: " + sizeToken;
					}
					if( isBlankWidth ) {
						reqSizeWidth = round(reqSizeHeight * aspectRatio); //w = h * ar, h = w / ar, ar = w / h
					}
				}
			}

			//  !w,h - modify higher value if it should keep aspect ratio and fit into limits
			if( isExclamationMark ){
				if ( (reqSizeHeight * aspectRatio) > reqSizeWidth ){
					reqSizeHeight = round (reqSizeWidth / aspectRatio);
				}
				if ( (reqSizeWidth / aspectRatio) > reqSizeHeight ){
					reqSizeWidth = round (reqSizeHeight * aspectRatio);
				}
			}
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
				errorMsg = "Rotation parameter must be decimal number between 0 and 360. "
					"You have entered: " + rotationString;
		}

		//check if converted value is supported
		if(!( rotation == 0 || rotation == 90 || rotation == 180 ||
			rotation == 270 || rotation == 360 )){
				errorNo = 501;
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
					errorMsg = "Currently, jpg is the only implemented format.";
				}
			}
			else {
					errorNo = 400;
					errorMsg = "Format must be one of: jpg, tif, png, gif, jp2 or pdf."
						" You have entered: " + format;
			}
		}
		numOfTokens++;

		if( !errorNo && session->loglevel >= 3 ){
			*(session->logfile) << "IIIF :: requested quality of image is: " << quality
				<< ", requested format is: " << format << endl;
		}
	}

	//TOO MUCH PARAMETERS, tell it to user and show him his request
	if( !errorNo && izer.hasMoreTokens() ){
		errorNo = 400;
		errorMsg = "Inserted query has more parameters. "
			"Syntax should be {identifier}/{region}/{size}/{rotation}/{quality}{.format} "
			"You have entered: " + argument;
	}
	//NOT ENOUGH PARAMETERS
	if( !errorNo && numOfTokens < 5 ){
		errorNo = 400;
		errorMsg = "Inserted query has not enough parameters. "
			"Syntax should be {identifier}/{region}/{size}/{rotation}/{quality}{.format} "
			"You have entered: " + argument;
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
	  char str[1024];
	  snprintf( str, 1024,
	      "Server: iipsrv/%s\r\n"
		  "Cache-Control: no-cache\r\n"
	      "Content-Type: text/plain\r\n"
		  "Status: %d %s\r\n"
		  "\r\n"
	      "%s",
		  VERSION, errorNo, statusMsg.c_str(), (statusMsg + ": " + errorMsg).c_str() );
	  session->out->printf((const char*) str);
	  session->out->flush();

	  *(session->logfile) << "IIIF :: Parsing error occured. " << errorMsg
		  << " Entered argument was " << argument << endl;
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
	std::stringstream xmlStringStream;
	xmlStringStream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;
	xmlStringStream << "<info xmlns=\"http://library.stanford.edu/iiif/image-api/ns/\">" << endl;
	xmlStringStream << "<identifier>" << filename << "</identifier>" << endl;
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


    char str[1024];
    snprintf( str, 1024,
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
  else if( suffix == "info.json" ){
	std::stringstream jsonStringStream;
	jsonStringStream << "{" << endl;
	jsonStringStream << "\"identifier\" : \"" << filename << "\"," << endl;
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


	char str[1024];
    snprintf( str, 1024,
	      "Server: iipsrv/%s\r\n"
	      "Content-Type: application/json\r\n"
	      "Cache-Control: max-age=%d\r\n"
	      "Last-Modified: %s\r\n"
	      "\r\n"
	      "%s",
		  VERSION, MAX_AGE,(*session->image)->getTimestamp().c_str(), jsonStringStream.str().c_str() );
    session->out->printf( (const char*) str );
    session->response->setImageSent();
    return;
  }

  // IMAGE REQUEST (all requests other than info requests are considered image requests)
  else {

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
	//we will not limit resolution
	session->view->setMaxSize(UINT_MAX);
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

    char str[1024];
    snprintf( str, 1024,
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


	// *** RESIZE IMAGE ***

	// Resize our image as requested. Use the interpolation method requested in the server configuration - bilinear default
	if( (reqSizeWidth != complete_image.width) || (reqSizeHeight != complete_image.height) ){
	  if( session->loglevel >= 5 ){
		  *(session->logfile) << "Resizing is required." << endl;
	  }
      //if 16 bits per channel, change it to 8bpc
      if(complete_image.bpc == 16) filter_contrast( complete_image, 1.0 );
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


	// *** ROTATE IMAGE ***

	if( (int) rotation % 90 == 0 && (int) rotation % 360 != 0 ){

		//if 16 bits per channel, change it to 8bpc
		if(complete_image.bpc == 16) filter_contrast( complete_image, 1.0 );

		Timer rotationTimer;
		if( session->loglevel >= 4 ){
			rotationTimer.start();
		}

		int i=0;
		unsigned char* buf;
		unsigned char* rotatedImage;
		buf = (unsigned char*) complete_image.data;
		rotatedImage = new unsigned char[complete_image.dataLength];

		//rotate 90
		if ((int) rotation % 360 == 90){
			for (int wid = complete_image.width; wid > 0; wid--){
				for (int hei = complete_image.height; hei > 0; hei--){
					for(int chan = 0; chan < complete_image.channels; chan++){
						rotatedImage[i*complete_image.channels + chan] = buf[(complete_image.width * hei - wid )*complete_image.channels + chan];
					}
					i++;
				}
			}
		}

		//rotate 270
		if( (int) rotation % 360 == 270 ){
			for (int wid = 1; wid <= complete_image.width; wid++){
				for (int hei = 1; hei <= complete_image.height; hei++){
					for(int chan = 0; chan < complete_image.channels; chan++){
						rotatedImage[i*complete_image.channels + chan] = buf[(complete_image.width * hei - wid )*complete_image.channels + chan];
					}
					i++;
				}
			}
		}

		//rotate 180, don't touch channel mathgic inside
		if( (int) rotation % 360 == 180 ){
			while ( i < complete_image.dataLength ){
				rotatedImage[i] = buf[complete_image.dataLength - i];		//blue channel
				rotatedImage[i+1] = buf[complete_image.dataLength - i - 2];	//red channel
				rotatedImage[i+2] = buf[complete_image.dataLength - i - 1];	//green channel
				i = i + 3;
			}
		}

		//delete old image
		if (complete_image.memoryManaged) {
			delete [] complete_image.data;
		}
		//set new image
		complete_image.data = rotatedImage;
		complete_image.memoryManaged = 1;

		//for 90 and 270 rotation swap width and height
		if( (int)rotation % 180 == 90 ){
			unsigned int tmp = reqSizeHeight;
			reqSizeHeight = reqSizeWidth;
			reqSizeWidth = tmp;
			tmp = complete_image.height;
			complete_image.height = complete_image.width;
			complete_image.width = tmp;
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
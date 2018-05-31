/**
 *  ofxImageSequence.h
 *
 * Created by James George, http://www.jamesgeorge.org
 * in collaboration with Flightphase http://www.flightphase.com
 *		- Updated for 0.8.4 by James George on 12/10/2014 for Specular (http://specular.cc) (how time flies!) 
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * ----------------------
 *
 *  ofxImageSequence is a class for easily importing a series of image files
 *  and accessing them like you would frames of a movie.
 *  
 *  This class loads only textures to the graphics card and does not store pixel data in memory. This helps with
 *  fast, random access drawing of seuqences
 *  
 *  Why would you use this instead of a movie file? A few reasons,
 *  If you want truly random frame access with no lag on large images, ofxImageSequence is a good way to go
 *  If you need a movie with alpha channel the only readily available codec is Animation (PNG) which is slow at large resolutions, so this class can help with that
 *  If you want to easily access frames based on percents this class makes that easy
 * 
 * //TODO: Extend ofBaseDraws
 * //TODO: experiment with storing pixels intead of textures and doing upload every frame
 * //TODO: integrate ofDirectory to API
 */

#pragma once

#include "ofMain.h"

class ofxImageSequenceImporter;
class ofxImageSequenceExporter;

typedef enum _SequenceStatus {
	Status_Undefined,
	Status_Loading,
	Status_Importing,
	Status_Exporting,
	Status_Ready,
} SequenceStatus;

class ofxImageSequence : public ofBaseHasTexture{
  public:

	ofxImageSequence();
	~ofxImageSequence();

	void setMaxFrames(int maxFrames); //set to limit the number of frames. 0 or less means no limit

	/**
	 *	Use this function to import sequences formatted like
	 *
	 *	path/to/images/myImage004.jpg
	 *	path/to/images/myImage005.jpg
	 *	path/to/images/myImage006.jpg
	 *	path/to/images/myImage007.jpg
	 *
	 *	for this sequence the parameters would be:
	 *	prefix		=> "path/to/images/myImage"
	 *	filetype	=> "jpg"
	 *	startIndex	=> 4
	 *	endIndex	=> 7
	 *	numDigits	=> 3
	 */
	
	// import
	void enableThreadedImport(bool enable);
	void pauseImport();
	void resumeImport();
	void cancelImport();
	void deleteImportThread();
	bool importSequence(string prefix, string filetype, int startIndex, int endIndex);
	bool importSequence(string prefix, string filetype, int startIndex, int endIndex, int numDigits);
	bool importSequence(string folder);
	void setExtensionToImport(string prefix);
	bool isImported();						//returns true if the sequence has been imported
//	bool isImporting();						//returns true if importing during thread
	void completeImporting();
	float percentImported();
	ofEvent<ofxImageSequence&> importComplete_event;

	// export
	void enableThreadedExport(bool enable);
	void pauseExport();
	void resumeExport();
	void cancelExport();
	void deleteExportThread();
	bool isExported();						//returns true if the sequence has been exported
//	bool isExporting();						//returns true if exporting during thread
	void completeExporting();
	bool exportSequence(string folder, string extension);
	void exportAllFrames();
	void enableOverwriteOnExport(bool enable);
	float percentExported();
	ofEvent<ofxImageSequence&> exportComplete_event;

	// load
	void startLoading(unsigned long length);
	void addFrame(ofPixels& imageToSave, string name = "");
	bool isLoaded();
	bool isLoading();
	bool completeLoading();
	float percentLoaded();
	unsigned long getLoadedFrameIndex() { return lastLoadedFrame; }
//	unsigned long getExpectedLength() { return expectedLength; }
	ofEvent<ofxImageSequence&> loadComplete_event;

	//	void exportFrame(int imageIndex);	

	void setFrameRate(float rate); //used for getting frames by time, default is 30fps	

	//these get textures, but also change the
	OF_DEPRECATED_MSG("Use getTextureForFrame instead.",   ofTexture* getFrame(int index));		 //returns a frame at a given index
	OF_DEPRECATED_MSG("Use getTextureForTime instead.",    ofTexture* getFrameForTime(float time)); //returns a frame at a given time, used setFrameRate to set time
	OF_DEPRECATED_MSG("Use getTextureForPercent instead.", ofTexture* getFrameAtPercent(float percent)); //returns a frame at a given time, used setFrameRate to set time

	ofTexture& getTextureForFrame(int index);		 //returns a frame at a given index
	ofTexture& getTextureForTime(float time); //returns a frame at a given time, used setFrameRate to set time
	ofTexture& getTextureForPercent(float percent); //returns a frame at a given time, used setFrameRate to set time

	//if usinsg getTextureRef() use these to change the internal state
	void setCurrentFrameIndex(int index);					
	void setFrameForTime(float time);			
	void setFrameAtPercent(float percent);
	void setExportQuality(ofImageQualityType quality);
	
	void setCreationTimeStamp(string ts);
	string getCreationTimeStamp() { return creationTimeStamp; }

	string getFilePath(int index);

	OF_DEPRECATED_MSG("Use getTexture() instead.", ofTexture& getTextureReference());

	virtual ofTexture& getTexture();
	virtual const ofTexture& getTexture() const;
	ofPixels& getPixels();

	virtual void setUseTexture(bool bUseTex){/* not used */};
	virtual bool isUsingTexture() const{return true;}

	int getFrameIndexAtPercent(float percent);	//returns percent (0.0 - 1.0) for a given frame
	float getPercentAtFrameIndex(int index);	//returns a frame index for a percent
	
    int getCurrentFrameIndex(){ return currentFrame; };
	int getTotalFrames();					//returns how many frames are in the sequence
	float getLengthInSeconds();				//returns the sequence duration based on frame rate
	
	float getWidth();						//returns the width/height of the sequence
	float getHeight();	

	void setMinMagFilter(int minFilter, int magFilter);

	// These must be private
	// Do not call directly they are supposed to be friend functions..
	// called internally from threaded loader
	// searches for all filenames based on load input
	// does not load to memory. will load to texture from disk.
	bool readFileNames();
	void preloadAllFrames();		//immediately loads all frames in the sequence, memory intensive but fastest scrubbing
	void loadFrameToTexture(int imageIndex);//allows you to load (cache) a frame to avoid a stutter when importing. use this to "read ahead" if you want

	// this returns percent no matter if importing, exporting or loading
	float getCompletionPercent();
	bool isReady();
	SequenceStatus getStatus();

  protected:
//	ofPtr<ofxImageSequenceImporter> importThread;
//	ofPtr<ofxImageSequenceExporter> exportThread;
	ofxImageSequenceImporter* importThread;
	ofxImageSequenceExporter* exportThread;

  private:
	vector<ofPixels> sequence;
	vector<string> filenames;
	vector<bool> loadFailed;
	ofTexture texture;

	ofImageQualityType exportQuality;
	string extensionImport, extensionExport;
	string folderToImport, folderToExport;
	bool overwrite;

	int nameCounter;
	int numberWidth;
	int maxFrames;
	bool useThreadToImport, useThreadToExport;
	bool imported, exported, loaded;

	float width, height;
	int currentFrame;
	int lastImportedFrame, lastExportedFrame, lastLoadedFrame, lastDisplayedFrame;
	float frameRate;
	
	int minFilter;
	int magFilter;

	unsigned long expectedLength;
	string creationTimeStamp;

	// private functions
	void deleteSequence();			//clears out all frames and frees up memory

	SequenceStatus status;
};





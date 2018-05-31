/**
 *  ofxImageSequence.cpp
 *
 * Created by James George, http://www.jamesgeorge.org
 * in collaboration with FlightPhase http://www.flightphase.com
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
 *  If you want truly random frame access with no lag on large images, ofxImageSequence allows it
 *  If you need a movie with alpha channel the only readily available codec is Animation (PNG) which is slow at large resolutions, so this class can help with that
 *  If you want to easily access frames based on percents this class makes that easy
 *  
 */

#include "ofxImageSequence.h"

class ofxImageSequenceExporter : public ofThread {
private:
	bool paused;
	bool exporting;
	ofxImageSequence& sequenceRef;
public:
	bool cancelExport;
	explicit ofxImageSequenceExporter(ofxImageSequence* seq)
		: sequenceRef(*seq)
		, exporting(true)
		, cancelExport(false)
		, paused(false)
	{
		ofLogVerbose("ofxImageSequenceExporter") << "Ctor";
	}

	~ofxImageSequenceExporter() {
		ofLogVerbose("ofxImageSequenceExporter") << "Dtor";
	}

	void pause() {
		stopThread();
		lock();
			cancelExport = true;
		unlock();
		paused = true;
	}
	void resume() {
		lock();
			cancelExport = false;
		unlock();
		paused = false;
		exporting = true;
		startThread(true);
	}

	void cancel() {
		lock();
			cancelExport = true;
		unlock();
		paused = false;
		exporting = false;
		waitForThread(true);
	}

	void threadedFunction() {
		ofAddListener(ofEvents().update, this, &ofxImageSequenceExporter::updateThreadedExport);
		sequenceRef.exportAllFrames();
		// task is finished. Let it close.
		exporting = false;
	}

	void updateThreadedExport(ofEventArgs& args) {
		if (exporting || paused) {
			return;
		}
		// task is finished. Let it close.
		ofRemoveListener(ofEvents().update, this, &ofxImageSequenceExporter::updateThreadedExport);
		ofLogVerbose("ofxImageSequenceExporter::updateThreadedExport") << "stop export thread.";
		stopThread();
		sequenceRef.completeExporting();
		sequenceRef.deleteExportThread();
	}
};


class ofxImageSequenceImporter : public ofThread
{
private:
	bool paused;
	bool importing;
	ofxImageSequence& sequenceRef;
public:
	bool cancelImport;
	explicit ofxImageSequenceImporter(ofxImageSequence* seq)
	: sequenceRef(*seq)
	, importing(true)
	, cancelImport(false)
	, paused(false)
	{
		ofLogVerbose("ofxImageSequenceImporter") << "Ctor";
//		startThread(true);	// this is a bad practice. 
							// imageSequence might want to call the ofxImageSequenceImporter instance 
							// and the instance returns NULL because the construction has not finished yet.
							// so its better to construct ofxImageSequenceImporter instance without any action
							// and call the tread to start when the instance is fully constructed.
	}
	
	~ofxImageSequenceImporter(){
		ofLogVerbose("ofxImageSequenceImporter") << "Dtor";
    }

	void pause() {
		ofLogVerbose("ofxImageSequenceImporter") << "pause";
		stopThread();
		lock();
			cancelImport = true;
		unlock();
		paused = true;
	}
	void resume() {
		ofLogVerbose("ofxImageSequenceImporter") << "resume";
		lock();
			cancelImport = false;
		unlock();
		paused = false;
		importing = true;
		startThread(true);
	}

    void cancel(){
		ofLogVerbose("ofxImageSequenceImporter") << "cancel";
		lock();
			cancelImport = true;
		unlock();
		paused = false;
		importing = false;
		waitForThread(true);
    }
    
	void threadedFunction(){
	
		ofAddListener(ofEvents().update, this, &ofxImageSequenceImporter::updateThreadedLoad);

		// load paths and check names
		if(!sequenceRef.readFileNames()){
			importing = false;
			return;
		}
	
		// load the images to memory
		sequenceRef.preloadAllFrames();
		// task is finished. Let it close.
		importing = false;
	}

	void updateThreadedLoad(ofEventArgs& args) {
		if (importing || paused) {
			return;
		}
		// task is finished. Let it close.
		ofRemoveListener(ofEvents().update, this, &ofxImageSequenceImporter::updateThreadedLoad);
		ofLogVerbose("ofxImageSequenceImporter::updateThreadedLoad") << "stop load thread.";
		stopThread();
		sequenceRef.completeImporting();
		sequenceRef.deleteImportThread();
	}
};

ofxImageSequence::ofxImageSequence()
{
	ofLogVerbose("ofxImageSequence") << "Ctor";
	status = Status_Undefined;

	importThread = NULL;
	exportThread = NULL;

	imported = false;
	exported = false;
	loaded = false;

	useThreadToImport = false;
	useThreadToExport = false;

	lastImportedFrame = -1;
	lastExportedFrame = -1;
	lastLoadedFrame = -1;
	lastDisplayedFrame = -1;

	expectedLength = 0;

	frameRate = 30.0f;
	currentFrame = 0;
	maxFrames = 0;
	nameCounter = 0;
	numberWidth = 3;
	overwrite = false;
	exportQuality = OF_IMAGE_QUALITY_BEST;
	creationTimeStamp = ofGetTimestampString();

	width = -1;
	height = -1;
	minFilter = -1;
	magFilter = -1;
}

ofxImageSequence::~ofxImageSequence()
{
	ofLogVerbose("ofxImageSequence") << "Dtor";
	// we must unload sequences, but, most important,
	// stop and close threads!
	deleteSequence();
}


/*
void addFrame(ofImage &img) {
addFrame(img.getPixelsRef());
}

void addFrame(ofVideoGrabber &cam) {
addFrame(cam.getPixelsRef());
}

void addFrame(ofVideoPlayer &player) {
addFrame(player.getPixelsRef());
}
*/

void ofxImageSequence::startLoading(unsigned long length)
{
	status = Status_Loading;
	loaded = false;
	expectedLength = length;
}

void ofxImageSequence::addFrame(ofPixels& imageToSave, string name) {

	if (extensionExport != "") {
		ofLogVerbose("ofxImageSequence::addFrame") << "no extension given. setting default (png).";
		extensionExport = "png";
	}

	string fileName;
	if (name == "") {
		fileName = ofToString(nameCounter, numberWidth, '0') + "." + extensionExport;
		nameCounter++;
	}
	else {
		fileName = name;
	}

	filenames.push_back(fileName);
	sequence.push_back(imageToSave);
	loadFailed.push_back(false);
	lastLoadedFrame = sequence.size();
}


bool ofxImageSequence::importSequence(string prefix, string filetype,  int startDigit, int endDigit)
{
	return importSequence(prefix, filetype, startDigit, endDigit, 0);
}

bool ofxImageSequence::importSequence(string prefix, string filetype,  int startDigit, int endDigit, int numDigits)
{
//	status = Status_Importing;
//
//	deleteSequence();
//
//	char imagename[1024];
//	stringstream format;
//	int numFiles = endDigit - startDigit + 1;
//	if(numFiles <= 0 ){
//		ofLogError("ofxImageSequence::importSequence") << "No image files found.";
//		return false;
//	}
//
//	if(numDigits != 0){
//		format <<prefix<<"%0"<<numDigits<<"d."<<filetype;
//	} else{
//		format <<prefix<<"%d."<<filetype; 
//	}
//	
//	for(int i = startDigit; i <= endDigit; i++){
//		sprintf_s(imagename, format.str().c_str(), i);
//		filenames.push_back(imagename);
//		sequence.push_back(ofPixels());
//		loadFailed.push_back(false);
//	}
//	
//	if (useThreadToImport) {
//		importThread = new ofxImageSequenceImporter(this);
////		importThread->startThread(true);
//	}
//	else {
//		preloadAllFrames();
//		completeImporting();
//	}
	return true;
}

bool ofxImageSequence::exportSequence(string _folder, string _extension)
{
	status = Status_Exporting;

	extensionExport = _extension;
	folderToExport =  "data/" +_folder;
	lastExportedFrame = 0;
	expectedLength = sequence.size();

	// before exporting we clear the previous export
//	ofDirectory exportFolder;
//	exportFolder.removeDirectory(folderToExport, true, true);
//	exportFolder.createDirectory(folderToExport, true);

	if (useThreadToExport) {
//		ofPtr<ofxImageSequenceExporter> exportThread(new ofxImageSequenceExporter(this));
		exportThread = new ofxImageSequenceExporter(this);
		exportThread->startThread(true);
	}
	else {
		exportAllFrames();
		completeExporting();
	}
	return true;
}


bool ofxImageSequence::importSequence(string _folder)
{
	status = Status_Importing;

	if (imported) {
		deleteSequence();
	}

	folderToImport = _folder;
	lastImportedFrame = 0;

	if(useThreadToImport){
//		ofPtr<ofxImageSequenceImporter> importThread(new ofxImageSequenceImporter(this));
		importThread = new ofxImageSequenceImporter(this);
		importThread->startThread(true);
	}
	else {
		if (readFileNames()) {
			completeImporting();
		}
	}
	return true;
}

void ofxImageSequence::completeImporting()
{
	if(sequence.size() == 0){
		ofLogError("ofxImageSequence::completeImporting") << "load failed with empty image sequence";
		// notify sequence container that sequence has finished importing.
		ofNotifyEvent(importComplete_event, *this);
		status = Status_Ready;	// Hack, this has to be after ofNotifyEvent. needs better solution.
		return;
	}

	width = sequence[0].getWidth();
	height = sequence[0].getHeight();


	// notify sequence container that sequence has finished importing.
	ofNotifyEvent(importComplete_event, *this);

	imported = true;
	lastImportedFrame = -1;
	status = Status_Ready;	// Hack, this has to be after ofNotifyEvent. needs better solution.

	ofLogVerbose("ofxImageSequence::completeImporting") << "import complete.";
	ofLogNotice() << "stored  " << sequence.size() << " frames";
}

void ofxImageSequence::completeExporting()
{
	if (sequence.size() == 0) {
		ofLogError("ofxImageSequence::completeExporting") << "export failed with empty image sequence";
		// notify sequence container that sequence has finished exporting.
		ofNotifyEvent(exportComplete_event, *this);
		status = Status_Ready;	// Hack, this has to be after ofNotifyEvent. needs better solution.
		return;
	}

	// notify sequence container that sequence has finished exporting.
	ofNotifyEvent(exportComplete_event, *this);

	exported = true;
	lastExportedFrame = -1;
	status = Status_Ready;

	ofLogVerbose("ofxImageSequence::completeExporting") << "export complete.";
}

bool ofxImageSequence::completeLoading()
{
	if (sequence.size() == 0) {
		ofLogError("ofxImageSequence::completeLoading") << "load failed with empty image sequence";
		// probably we would need to destroy 0 frame sequences.

		// notify sequence container that sequence has finished loading.
		ofNotifyEvent(loadComplete_event, *this);
		loaded = true;
		return false;
	}

	width = sequence[0].getWidth();
	height = sequence[0].getHeight();

	// notify sequence container that sequence has finished loading.
	ofNotifyEvent(loadComplete_event, *this);

	loaded = true;
	lastLoadedFrame = -1;

	// not sure why this was here.. probably not needed.
	//texture.clear();
	//loadFrameToTexture(0);

	status = Status_Ready;
	ofLogVerbose("ofxImageSequence::completeLoading") << "load complete.";
	ofLogNotice() << "stored  " << sequence.size() << " frames";
	return true;
}

bool ofxImageSequence::readFileNames()
{
    ofDirectory dir;
	dir.allowExt("png");
	dir.allowExt("jpg");
	dir.allowExt("jpeg");
	dir.allowExt("tiff");
	dir.allowExt("bmp");

	if(extensionImport != ""){
		dir.allowExt(extensionImport);
	}
	
	if(!ofFile(folderToImport).exists()){
		ofLogError("ofxImageSequence::readFileNames") << "Could not find folder " << folderToImport;
		return false;
	}

	int numFiles = 0;
	if(maxFrames > 0){
		numFiles = MIN(dir.listDir(folderToImport), maxFrames);
	}
	else{	
		numFiles = dir.listDir(folderToImport);
	}

    if(numFiles == 0) {
		ofLogError("ofxImageSequence::readFileNames") << "No image files found in " << folderToImport;
		return false;
	}

    // read the directory for the images
	#ifdef TARGET_LINUX
	dir.sort();
	#endif
	
	// we can't clear because if we pause and resume we will brake storage.
	filenames.clear();
//	sequence.clear();
//	loadFailed.clear();

	for(int i = 0; i < numFiles; i++) {
		string filename = ofFilePath::getFileName(dir.getPath(i));
		extensionImport = ofFilePath::getFileExt(dir.getPath(i));
		ofStringReplace(filename, "." + extensionImport, "");

        filenames.push_back(filename);
		//sequence.push_back(ofPixels());
		//loadFailed.push_back(false);
    }

	expectedLength = numFiles;
	return true;
}

//set to limit the number of frames. negative means no limit
void ofxImageSequence::setMaxFrames(int newMaxFrames)
{
	maxFrames = MAX(newMaxFrames, 0);
	if(imported){
		ofLogError("ofxImageSequence::setMaxFrames") << "Max frames must be called before load";
	}
}

void ofxImageSequence::setExtensionToImport(string ext)
{
	extensionImport = ext;
}

void ofxImageSequence::enableThreadedImport(bool enable){
	ofLogVerbose("ofxImageSequence::enableThreadedImport") << enable;
	useThreadToImport = enable;
}

void ofxImageSequence::enableThreadedExport(bool enable) {
	ofLogVerbose("ofxImageSequence::enableThreadedExport") << enable;
	useThreadToExport = enable;
}

void ofxImageSequence::pauseImport()
{
	if (importThread != NULL) {
		importThread->pause();
		ofLogNotice("ofxImageSequence::pauseImport") << "paused";
	}
	else {
		ofLogVerbose("ofxImageSequence::pauseImport") << "nothing to do";
	}
}

void ofxImageSequence::resumeImport()
{
	if (importThread != NULL) {
		importThread->resume();
		ofLogNotice("ofxImageSequence::resumeImport") << "resumed";
	}
	else {
		ofLogVerbose("ofxImageSequence::resumeImport") << "nothing to do";
	}
}

void ofxImageSequence::cancelImport()
{
	ofLogNotice("ofxImageSequence::cancelImport");
	if(importThread != NULL){
		importThread->cancel();
//		completeImporting();	// done at updateThreadedLoad
//		deleteImportThread();	// done at updateThreadedLoad
		ofLogNotice("ofxImageSequence::cancelImport") << "canceled";
	}
	else {
		ofLogVerbose("ofxImageSequence::cancelImport") << "nothing to do";
	}
}

void ofxImageSequence::deleteImportThread()
{
	delete importThread;
	importThread = NULL;
}

void ofxImageSequence::pauseExport()
{
	if (exportThread != NULL) {
		exportThread->pause();
		ofLogNotice("ofxImageSequence::pauseExport") << "paused";
	}
	else {
		ofLogVerbose("ofxImageSequence::pauseExport") << "nothing to do";
	}
}

void ofxImageSequence::resumeExport()
{
	if (exportThread != NULL) {
		exportThread->resume();
		ofLogNotice("ofxImageSequence::resumeExport") << "resumed";
	}
	else {
		ofLogVerbose("ofxImageSequence::resumeExport") << "nothing to do";
	}
}

void ofxImageSequence::cancelExport()
{
	if (exportThread != NULL) {
		exportThread->cancel();
		ofLogNotice("ofxImageSequence::cancelExport")<< "canceled";
	}
	else {
		ofLogVerbose("ofxImageSequence::cancelExport") << "nothing to do";
	}
}

void ofxImageSequence::deleteExportThread()
{
	delete exportThread;
	exportThread = NULL;
}

void ofxImageSequence::setMinMagFilter(int newMinFilter, int newMagFilter)
{
	minFilter = newMinFilter;
	magFilter = newMagFilter;
	texture.setTextureMinMagFilter(minFilter, magFilter);
}

void ofxImageSequence::preloadAllFrames()
{
	//if(sequence.size() == 0){
	//	ofLogError("ofxImageSequence::preloadAllFrames") << "Calling preloadAllFrames on unitialized image sequence.";
	//	return;
	//}
	
	int framesToImport = expectedLength - lastImportedFrame;
	ofLogVerbose() << "lastImportedFrame " << lastImportedFrame;
	ofLogVerbose() << "sequence.size " << expectedLength;
	ofLogVerbose() << "framesToLoad " << framesToImport;

	for(int i = 0; i < framesToImport; i++){
		//threaded stuff
		if (useThreadToImport) {
			if (importThread == NULL) {
				ofLogError("ofxImageSequence::preloadAllFrames") << "importThread is NULL!";
				return;
			}
			importThread->lock();
			bool shouldExit = importThread->cancelImport;
			importThread->unlock();
			if (shouldExit) {
				return;
			}
		}

		string filepath = folderToImport + "/" + filenames[lastImportedFrame] + "." + extensionImport;

		if (ofFile(filepath).exists() == false) {
			ofLogError("ofxImageSequence::readFileNames") << "Could not find file " << filepath;
		}

		sequence.push_back(ofPixels());
		loadFailed.push_back(false);

		if (!ofLoadImage(sequence[lastImportedFrame], filepath)) {
			loadFailed[lastImportedFrame] = true;
			ofLogError("ofxImageSequence::preloadAllFrames") << "Image failed to load: " << filepath;
		}

		ofLogVerbose() << "imported " << filenames[lastImportedFrame];
		lastImportedFrame++;
		
		ofSleepMillis(5);
	}
}

void ofxImageSequence::exportAllFrames()
{
	if (expectedLength == 0) {
		ofLogError("ofxImageSequence::exportAllFrames") << "Calling exportAllFrames on uninitialized image sequence.";
		return;
	}

	int framesToExport = expectedLength - lastExportedFrame;
	ofLogVerbose() << "lastImportedFrame " << lastExportedFrame;
	ofLogVerbose() << "sequence.size " << expectedLength;
	ofLogVerbose() << "framesToLoad " << framesToExport;

	for (int i = 0; i < framesToExport; i++) {
		//threaded stuff
		if (useThreadToExport) {
			if (exportThread == NULL) {
				ofLogError("ofxImageSequence::exportAllFrames") << "exportThread is NULL!";
				return;
			}
			exportThread->lock();
			bool shouldExit = exportThread->cancelExport;
			exportThread->unlock();
			if (shouldExit) {
				return;
			}

			string filepath = folderToExport + "/" + creationTimeStamp + "/" + filenames[lastExportedFrame] + "." + extensionExport;

			ofFile file(filepath);
			if (file.exists()) {
				ofLogError("ofxImageSequence::exportAllFrames") << "file exists";
				if (overwrite) {
					ofLogVerbose("ofxImageSequence::exportAllFrames") << "overwriting";
					ofSaveImage(sequence[lastExportedFrame], filepath);
				}
				else {
					ofLogVerbose() << filepath;
				}
			}
			else {
				// need to add error checking ofSaveImage does not have any
				ofSaveImage(sequence[lastExportedFrame], filepath, exportQuality);
			}
		}

		ofLogVerbose() << "exported " << filenames[lastExportedFrame];
		lastExportedFrame++;

		ofSleepMillis(5);
	}
}

void ofxImageSequence::enableOverwriteOnExport(bool enable)
{
	overwrite = enable;
}

float ofxImageSequence::percentExported() {
	if (expectedLength > 0) {
		return 1.0 * lastExportedFrame / expectedLength;
	}
	return 0.0;
}

float ofxImageSequence::percentImported() {
	if (expectedLength > 0) {
		return 1.0 * lastImportedFrame / expectedLength;
	}
	return 0.0;
}

float ofxImageSequence::percentLoaded()
{
	if (expectedLength > 0) {
		return 1.0 * lastLoadedFrame / expectedLength;
	}
	return 0.0;
}

float ofxImageSequence::getCompletionPercent()
{
	switch (status)
	{
	case Status_Undefined:	return 0.0;
	case Status_Loading:	return percentLoaded();
	case Status_Importing:	return percentImported();
	case Status_Exporting:	return percentExported();
	case Status_Ready:		return 1.0;
	default:				return 0.0f;
	}
}


void ofxImageSequence::setCurrentFrameIndex(int index)
{
	if (isReady() == false || getTotalFrames() <= 0) {
		ofLogError("ofxImageSequence::setCurrentFrameIndex") << "Sequence is not ready.";
		return;
	}

	index = MAX(0, index);
	index %= getTotalFrames();

	loadFrameToTexture(index);
	currentFrame = index;
}

void ofxImageSequence::loadFrameToTexture(int imageIndex)
{
	if(lastDisplayedFrame == imageIndex){
		return;
	}

	if(imageIndex < 0 || imageIndex >= sequence.size()){
		ofLogError("ofxImageSequence::loadFrameToTexture") << "Calling a frame out of bounds: " << imageIndex;
		return;
	}

	if(!sequence[imageIndex].isAllocated() && !loadFailed[imageIndex]){
		if(!ofLoadImage(sequence[imageIndex], filenames[imageIndex])){
			loadFailed[imageIndex] = true;
			ofLogError("ofxImageSequence::loadFrameToTexture") << "Image failed to load: " << filenames[imageIndex];
		}
	}

	if(loadFailed[imageIndex]){
		return;
	}

	texture.loadData(sequence[imageIndex]);

	lastDisplayedFrame = imageIndex;
}

float ofxImageSequence::getPercentAtFrameIndex(int index)
{
	return ofMap(index, 0, sequence.size()-1, 0, 1.0, true);
}

float ofxImageSequence::getWidth()
{
	return width;
}

float ofxImageSequence::getHeight()
{
	return height;
}

void ofxImageSequence::deleteSequence()
{
	cancelImport();
	cancelExport();

	sequence.clear();
	filenames.clear();
	loadFailed.clear();
	texture.clear();
}

void ofxImageSequence::setFrameRate(float rate)
{
	frameRate = rate;
}

string ofxImageSequence::getFilePath(int index){
	if(index > 0 && index < filenames.size()){
		return filenames[index];
	}
	ofLogError("ofxImageSequence::getFilePath") << "Getting filename outside of range";
	return "";
}

int ofxImageSequence::getFrameIndexAtPercent(float percent)
{
    if (percent < 0.0 || percent > 1.0) percent -= floor(percent);

	return MIN((int)(percent*sequence.size()), sequence.size()-1);
}

//deprecated
ofTexture& ofxImageSequence::getTextureReference()
{
	return getTexture();
}

//deprecated
ofTexture* ofxImageSequence::getFrameAtPercent(float percent)
{
	setFrameAtPercent(percent);
	return &getTexture();
}

//deprecated
ofTexture* ofxImageSequence::getFrameForTime(float time)
{
	setFrameForTime(time);
	return &getTexture();
}

//deprecated
ofTexture* ofxImageSequence::getFrame(int index)
{
	setCurrentFrameIndex(index);
	return &getTexture();
}

ofTexture& ofxImageSequence::getTextureForFrame(int index)
{
	setCurrentFrameIndex(index);
	return getTexture();
}

ofTexture& ofxImageSequence::getTextureForTime(float time)
{
	setFrameForTime(time);
	return getTexture();
}

ofTexture& ofxImageSequence::getTextureForPercent(float percent){
	setFrameAtPercent(percent);
	return getTexture();
}

void ofxImageSequence::setFrameForTime(float time)
{
	float totalTime = sequence.size() / frameRate;
	float percent = time / totalTime;
	return setFrameAtPercent(percent);	
}

void ofxImageSequence::setFrameAtPercent(float percent)
{
	setCurrentFrameIndex(getFrameIndexAtPercent(percent));	
}

void ofxImageSequence::setExportQuality(ofImageQualityType q)
{
	exportQuality = q;
}

void ofxImageSequence::setCreationTimeStamp(string ts)
{
	creationTimeStamp = ts;
}

ofTexture& ofxImageSequence::getTexture()
{
	return texture;
}

const ofTexture& ofxImageSequence::getTexture() const
{
	return texture;
}

ofPixels& ofxImageSequence::getPixels()
{
	return sequence[currentFrame];
}

float ofxImageSequence::getLengthInSeconds()
{
	return getTotalFrames() / frameRate;
}

int ofxImageSequence::getTotalFrames()
{
	return sequence.size();
}
//returns true if the sequence has been fully imported
bool ofxImageSequence::isImported(){						
    return imported;
}
// bool ofxImageSequence::isImporting(){
// 	return importThread != nullptr && (importThread->importing || importThread->paused);
// }
//returns true if the sequence has completed export
bool ofxImageSequence::isExported() {						
	return exported;
}
// bool ofxImageSequence::isExporting() {
// 	return exportThread != nullptr && (exportThread->exporting || exportThread->paused);
// }
bool ofxImageSequence::isLoaded() {
	return loaded;
}
bool ofxImageSequence::isLoading() {
	return status == Status_Loading;
}
bool ofxImageSequence::isReady()
{
	return status == Status_Ready;
}
SequenceStatus ofxImageSequence::getStatus()
{
	return status;
}

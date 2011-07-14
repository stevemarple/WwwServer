//#define __STDC_LIMIT_MACROS
//#include <stdint.h>
#include <avr/pgmspace.h>

#include <limits.h>
#include <WwwServer.h>

char WwwServer::urlStart[] = {"http://"};
char WwwServer::location[] = {"Location: "};
char WwwServer::contentType[] = {"Content-Type: "};
char WwwServer::textHtml[] = {"text/html"};
//char WwwServer::textPlain[] = {"text/plain"};

char WwwServer::htmlToTitle[] = {"<html><head><title>"};
char WwwServer::titleToH1[] = {"</title></head>\n<body><h1>"};
char WwwServer::closeH1[] = {"</h1>"};
char WwwServer::closeBodyHtml[] = {"</body></html>"};

const char* WwwServer::methodNames[] = {
  "HEAD",
  "GET",
  // "POST",
  "PUT",
  "DELETE",
  NULL
};

const char* WwwServer::responseText[] = {
  "200 OK",
  //"201 Created",
  "301 Moved Permanently",
  "307 Temporary Redirect",
  "400 Bad Request",
  //"401 Unauthorized",
  "403 Forbidden",
  "404 Not Found",
  "414 Request-URI Too Long",
  "500 Internal Server Error",
  NULL
};

const char* WwwServer::errorDocumentKeys[] = {
  "error document 200",
  //"error document 201",
  "error document 301",
  "error document 307",
  "error document 400",
  //"error document 401",
  "error document 403",
  "error document 404",
  "error document 414",
  "error document 500",
  NULL
};

const char* WwwServer::handlerNames[] = {
  "default",
  "forbidden",
  "moved permanently",
  "temporary redirect",
  "status",
  "cgi",
  NULL, // "directory listing", NULL ensures internal use only
  NULL
};

WwwServer::WwwServer(const char* filename, uint16_t port) \
  : _server(port), _ini(filename)
{
  _port = port;

  _stats.requestStarted = 0UL;
  _stats.requestCount = 0UL;
  _stats.requestTimeWorstCase = 0UL;
  _stats.taskTimeWorstCase = 0UL;
  _stats.taskWorstCaseState = -1;

  // Ensure clean starting point
  disconnect();
}

boolean WwwServer::begin(char *buffer, int len)
{
  // Check IniFile can be opened and validates
  if (!_ini.open())
    return false;

  if (!_ini.validate(buffer, len))
    return false;
  
  // _ini.close();
  _server.begin();
  return true;
}

void WwwServer::disconnect(void)
{
  if (_client)
    _client.stop();
  if (_file)
    _file.close();
  _state = stateNoClient;
  _stateData = 0;
  _method = -1;
  _url[0] = '\0';
  _queryString[0] = '\0';
  _handler = handlerDefault;
  _statusCode = statusOK;
  _isAuthenticated = false;

  // Reset state finormation for reading from ini files
  _iniState = IniFileState();
}

// Return number of characters, or negative if an error
int WwwServer::readLineFromClient(char* buffer, int len)
{
  int i = 0;
  char c;
  if (len < 3)
    return errorBufferTooShort;
  
  while (_client.connected() && _client.available()) {
    c = _client.read();
    if (c == '\n') {
      // end of line discard any following '\r'
      if (_client.peek() == '\r')
	_client.read();
      buffer[i] = '\0';
      return i;
    }

    if (c == '\r') {
      // end of line discard any following '\n'
      if (_client.peek() == '\n')
	_client.read();
      buffer[i] = '\0';
      return i;
    }

    buffer[i++] = c;
    if (i == len) {
      buffer[len-1] == '\0';
      return errorBufferTooShort;
    }
  }
  buffer[i] = '\0';
  return i;
}

// State information for getIniFileValueForUrl(). These variables with
// static linkage really should be inside WwwServer::processRequest(),
// with static storage but that produces a compiler error (undefined
// reference to `__cxa_guard_acquire')
int8_t WwwServer::processRequest(char* buffer, int len)
{
  int i = 0;
  unsigned long startMicros = micros();
  uint8_t initialState = _state;
  if (len < 1) {
    i = errorBufferTooShort;
    _state = stateSendingStatusCode;
    _stateData = 0;
  }

  // Check if the client disconnected
  if (_state != stateNoClient && !_client) {
    _state = stateDisconnecting;
  }
  
  switch (_state) {
  case stateNoClient:
    // check if a new client is waiting
    _client = _server.available();
    if (!_client) 
      break;
    
    // TO DO: Check if client is allowed access
    _state = stateReadingMethod;
    break;
    
  case stateReadingMethod:
    i = parseMethodUrlQueryString(buffer, len);
    if (i < 0) {
      if (i == errorRequestUriTooLong)
	_statusCode = statusRequestUriTooLong;
      else
	_statusCode = statusBadRequest;
      _state = stateSendingStatusCode;
      break;
    }
    _state = stateGettingHandlerSetUp;
    break;

  case stateGettingHandlerSetUp:
    // Reset the variables which hold state information for
    // getIniFileValueForUrl()
    _iniState = IniFileState();
    _lastSlash = NULL;
    _state = stateGettingHandler;
    break;
    
  case stateGettingHandler:
    // Figure out how to process this request. Send file, an error
    // document, redirect etc
    
    if (setHandler(buffer, len) == 0)
      break; // Not completed yet
    switch (_handler) {
    case handlerDefault:
      _state = stateReadingHeaders;
      break;
    case handlerMovedPermanently:
      _statusCode = statusMovedPermanently;
      _state = stateFindingLocationSetUp;
      break;
    case handlerTemporaryRedirect:
      _statusCode = statusTemporaryRedirect;
      _state = stateFindingLocationSetUp;
      break;
    default:
    case handlerForbidden:
      _statusCode = statusForbidden;
      _state = stateFindingErrorDocumentSetUp;
      break;
    }
    break;

  case stateReadingHeaders:
    // Parse the request headers, silently accept truncated ones
    i = readLineFromClient(buffer, len);

    if (i == 0) {
      // TO DO: will depend upon handler
      _state = stateUrlToFilename;
      break; // Empty line, stop processing headers
    }

    // TO DO: save any useful headers, like authentication ones
    char *p;
    if ((p = replaceCharByNull(buffer, ':')) != NULL)
      if (strcmp(buffer, "Authorization") == 0)
	// TO DO: fix authentication
	_isAuthenticated = true;
    break;
    
    // If the direct mapping between URLs and filenames is lost this
    // causes problems for directory listings.   
  case stateUrlToFilename:
    switch (urlToFilename(buffer, len)) {
    case errorNoError:
      _state = stateSendingStatusCode;
      break;
    case errorFileMissing:
      _statusCode = statusNotFound;
      _state = stateFindingErrorDocumentSetUp;
      break;
    case errorDirectoryNoTrailingSlash:
      _state = stateRedirectingToDirectory;
      break;
    default:
      _statusCode = statusInternalServerError;
      _state = stateSendingStatusCode;
      break;
    }
    break;

  case stateRedirectingToDirectory:
    redirectToDirectory();
    _state = stateSendingStatusCode;
    break;

  case stateFindingLocationSetUp:
    // Reset the variables which hold state information for
    // getIniFileValueForUrl()
    _iniState = IniFileState();
    _lastSlash = NULL;
    _state = stateFindingLocation;
    
  case stateFindingLocation:
    // Replace URL with the redirect target URL.
    if (findLocation(buffer, len) == 0)
      break; // Not completed yet
    
    _state = stateSendingStatusCode;
    break;

  case stateFindingErrorDocumentSetUp:
    // Reset the variables which hold state information for
    // getIniFileValueForUrl()
    _iniState = IniFileState();
    _lastSlash = NULL;
    _state = stateFindingErrorDocument;
    
  case stateFindingErrorDocument:
    // Replace error URL with the error document filename, otherwise
    // erase URL
    if (findErrorDocument(buffer, len) == 0)
      break; // Not completed yet
    _state = stateSendingStatusCode;
    break;
    
  case stateSendingStatusCode:
    sendStatusCode();
    switch (_handler) {
    case handlerDefault:
    case handlerDirectoryListing:
    case handlerForbidden:
    case handlerMovedPermanently:
    case handlerTemporaryRedirect:
      _state = stateRunningDefaultHandler;
      break;

    case handlerStatus:
      _state = stateRunningStatusHandler;
      break;
      
    default:
      _statusCode = statusInternalServerError;
      strncpy(_url, "Unknown handler in state stateSendingStatusCode",
	      WWW_SERVER_MAX_URL_LEN);
      _url[WWW_SERVER_MAX_URL_LEN] = '\0';
      _state = stateRunningDefaultHandler;
      break;
    }
    break;
    
  case stateRunningDefaultHandler:
    _state = defaultHandler(buffer, len);
    break;

  case stateSendingFileMimeTypeSetUp:
    // Reset the variable which holds state information for
    // _ini.getValue()
    _iniState = IniFileState();
    _state = stateSendingFileMimeType;
    break;
    
  case stateSendingFileMimeType:
    switch (sendFileMimeType(false, buffer, len)) {
    case 0:
      break; // Not completed yet
    case 1:
      // Found and sent
      _state = stateSendingFile;
      break;
    default:
      // Not found, try the default
      _state = stateSendingDefaultMimeTypeSetUp;
      break;
    }
    break;

  case stateSendingDefaultMimeTypeSetUp:
    // Reset the variable which holds state information for
    // _ini.getValue()
    _iniState = IniFileState();
    _state = stateSendingDefaultMimeType;
    break;

  case stateSendingDefaultMimeType:
    if (sendFileMimeType(true, buffer, len))
      // Found and sent
      _state = stateSendingFile;
    break;
    
  case stateSendingFile:
    // make repeated calls to send files
    if (sendFile(buffer, len))
      _state = stateClosingConnection;
    break;

  case stateSendingDirectoryListingHeader:
    sendDirectoryListingHeader();
    _state = stateSendingDirectoryListingBody;
    break;

  case stateSendingDirectoryListingBody:
    if (sendDirectoryListingBody(buffer, len))
      _state = stateSendingDirectoryListingFooter;
    break;

  case stateSendingDirectoryListingFooter:
    sendDirectoryListingFooter();
    _state = stateClosingConnection;
    break;

  case stateRunningStatusHandler:
    sendStatus();
    _state = stateClosingConnection;
    break;

  case stateClosingConnection:
    // give the web browser time to receive the data
    if (micros() < _stateData + 2000000UL)
      break;
    _state = stateDisconnecting;
    break;

  case stateDisconnecting:
    // close the connection:
    disconnect();
    break;
    
  default:
    i = errorUnknownState;
    _statusCode = statusBadRequest;
    _state = stateSendingStatusCode;
    //_stateData = 0;
    break;
  }

  if (_state != initialState) {
    if (_state == stateClosingConnection)
      _stateData = micros();
    else
      _stateData = 0;
  }

  // Don't include details when nothing was done
  if (_state != stateNoClient || initialState != stateNoClient)
    updateStats(startMicros, initialState);

  return _state;
}

int8_t WwwServer::parseMethodUrlQueryString(char* buffer, int len)
{
  int i = readLineFromClient(buffer, len);
  char *p, *q;
  // Shortest valid request is "GET /"
  if (i < 0) 
    return errorRequestUriTooLong;
  
  // find end of method
  if ((p = replaceCharByNull(buffer, ' ')) == NULL)
    return errorBadRequest;

  if ((i = findString(methodNames, buffer)) == -1)
    return errorBadRequest;
  _method = i;
    
  // find end of URL; may be terminated with a space or a '?'
  ++p; // go to start of URL
  if ((q = replaceCharByNull(p, '?')) == NULL) 
    // no query string
    replaceCharByNull(p, ' ');
  else {
    // found a query string
    replaceCharByNull(++q, ' ');
    strncpy(_queryString, q, WWW_SERVER_MAX_QUERY_LEN);
    _queryString[WWW_SERVER_MAX_QUERY_LEN] = '\0';
  }

  strncpy(_url, p, WWW_SERVER_MAX_URL_LEN);
  _url[WWW_SERVER_MAX_URL_LEN] = '\0';

  // Check for bad URLs
  if (_url[0] != '/') {
    _url[0] = '\0';
    return errorBadRequest; // not absolute as it should be
  }

  strncpy(_url, p, WWW_SERVER_MAX_URL_LEN);
  _url[WWW_SERVER_MAX_URL_LEN] = '\0'; // ensure _url is terminated
  return errorNoError;
}

int8_t WwwServer::setHandler(char* buffer, int len)
{
  int8_t done = getIniFileValueForUrl("handler", buffer, len);
  int i;
  switch (done) {
  case 0:
    break; // still looking for the handler
  case 1:
    if ((i = findString(handlerNames, buffer)) != -1)
      _handler = i;
    break;
  default:
    _handler = handlerForbidden; // no handler, or unknown
    break;
  }
  return done;
}

int8_t WwwServer::findString(const char** stringTable, char* str) const
{
  int8_t i = 0;
  while (stringTable[i]) {
    if (strcmp(str, stringTable[i]) == 0) { 
      return i;
    }
    ++i;
  };
  
  return -1;
}

// Gradually widen the scope of the URL by dropping parts off the end
// of the path until a match is found, or the URL has been shortened
// to "/". Return the value from getIniFile().  Before first call set
// _lastSlash to NULL and initialise _iniState. _lastSlash is the
// location of the last '/' character replaced by '\0'.
int8_t WwwServer::getIniFileValueForUrl(const char* key, char* buffer, int len)
{
  int8_t done;
  _ini.open();
  if (_lastSlash == NULL || _lastSlash != _url)
    // Look for the key in the ini file section for the current value
    // of _url. Whilst _lastSlash == NULL this is the complete URL. If not
    // found for the original URL it will be a shortened version.
    done = _ini.getValue(_url, key, buffer, len);
  else
    // Could not find for any of the intermediate directories so try /
    done = _ini.getValue("/", key, buffer, len);

  char *rs;
  switch (done) {
  case IniFile::errorSectionNotFound:
  case IniFile::errorKeyNotFound:
    if (_lastSlash == _url || strcmp(_url, "/") == 0) {
      // Tried all possiblities, even "/" and still not found
      // anything. Also test for condition where original URL is "/"
      // (hence _lastSlash == NULL)
      if (_lastSlash)
	*_lastSlash = '/'; // Restore
      break;
    }
    
    // Not found for this URL, but can try a shorter version
    rs = strrchr(_url, '/');
    if (_lastSlash)
      *_lastSlash = '/'; // replace previously deleted /
    
    if (rs) {
      done = 0; // try again!
      _iniState = IniFileState(); // reset the readLine state
      if (rs != _url) {
	// Another / exists and it isn't the leading one
	*rs = '\0'; // Terminate to make a shorter _url
	_lastSlash = rs; // Remember where to restore
      }
      else
	// Have to try "/" now, but cannot replace the leading '/'
	// with '\0', and cannot replace the character after it as no
	// way to restore it later. Set the _lastSlash to the start of the
	// URL to mark this special case
	_lastSlash = _url;
    }
    break;
  case 1:
    // found an entry for key, restore _url to original value
    if (_lastSlash)
      *_lastSlash = '/';
    break;
  }
  
  // _ini.close();
  return done;
}

char* WwwServer::replaceCharByNull(char *s, char c)
{
  while (s && *s != '\0') {
    if (*s == c) {
      *s = '\0';
      return s;
    }
    ++s;
  }
  return NULL;
}

// Add trailing slash to URL and redirect
void WwwServer::redirectToDirectory(void)
{
  int i = strlen(_url);
  if (i >= WWW_SERVER_MAX_URL_LEN) {
    _statusCode = statusRequestUriTooLong;
    return;
  }
  _url[i] = '/';
  _url[++i] = '\0';
  _statusCode = statusMovedPermanently;
}

// Find the target URL for redirections. It is an error if the location
// cannot be found.
int8_t WwwServer::findLocation(char* buffer, int len)
{
  const char locationTooLong[] = "Location URL too long";
  const char locationNotSpecified[]  =
    "Redirection specified but location not found";

  int8_t done = getIniFileValueForUrl("location", buffer, len);
  switch (done) {
  case 0:
    break; // still looking
    
  case 1:
    if (strlen(buffer) <= WWW_SERVER_MAX_URL_LEN)
      strcpy(_url, buffer); // May not start with http://..., fix later
    else {
      _statusCode = statusInternalServerError;
      strncpy(_url, locationTooLong, WWW_SERVER_MAX_URL_LEN);
      _url[WWW_SERVER_MAX_URL_LEN] = '\0';
    }
    break;

  default:
    strncpy(_url, locationNotSpecified, WWW_SERVER_MAX_URL_LEN);
    _url[WWW_SERVER_MAX_URL_LEN] = '\0';
    break;
  }

  return done;
}

// Replace error URL with the error document filename. If not found
// erase URL.
int8_t WwwServer::findErrorDocument(char* buffer, int len)
{
  int8_t done = getIniFileValueForUrl(errorDocumentKeys[_statusCode], buffer,
				      len);
  if (done != 0)
    if (strlen(buffer) <= WWW_SERVER_MAX_URL_LEN && SD.exists(buffer))
      strcpy(_url, buffer);
    else
      _url[0] = '\0';
  return done;
}

void WwwServer::sendStatusCode(void)
{
  _client.print("HTTP/1.1 ");
  _client.println(responseText[_statusCode]);
  _client.println("Connection: close");
}


// Cheat and store the filename back into the _url variable to
// save requiring another buffer.
int8_t WwwServer::urlToFilename(char* buffer, int len)
{
  // TO DO: map URLs to filenames?
  int8_t i = errorNoError;
  
  // Check if file exists, and if so if it is a directory
  if (_file)
    _file.close();
  _file = SD.open(_url, FILE_READ);
  if (!_file) 
    i = errorFileMissing;
  else {
    if (_file.isDirectory()) {
      if (_url[strlen(_url)-1] == '/')
	_handler = handlerDirectoryListing;
      else
	i = errorDirectoryNoTrailingSlash;
    }
  }
  
  return i;
}


// Handle the normal file and directory listing request. Also deal
// with redirects and forbidden actions which are similar
int8_t WwwServer::defaultHandler(char* buffer, int len)
{
  // Redirections
  if (_statusCode == statusMovedPermanently ||
      _statusCode == statusTemporaryRedirect) {
    _client.print(location);
    if (_url[0] == '/') {
      // Insert http:// and IP/port
      _client.print(urlStart);
      _client.print(Ethernet.localIP());
      if (_port != 80) {
	_client.print(':');
	_client.print(_port, DEC);
      }
    }
    _client.println(_url);
    _client.println();
    return stateClosingConnection;
  }
    
  if (_url[0] == '\0' || _statusCode == statusInternalServerError) {
    // No data to send (no file or error document) so send our own
    sendError();
    return stateClosingConnection;
  }

  switch (_handler) {
  case handlerDefault:
  case handlerForbidden:
  case handlerMovedPermanently:
  case handlerTemporaryRedirect:
    return stateSendingFileMimeTypeSetUp;

  case handlerDirectoryListing:
    return stateSendingDirectoryListingHeader;

  default:
    // how did we get here?
    _url[0] = '\0';
    _statusCode = statusInternalServerError;
    sendError("Unknown handler in defaultHandler()");
    return stateClosingConnection;
  }      
}

int8_t WwwServer::sendFileMimeType(boolean defaultType, char* buffer, int len)
{
  int8_t done = IniFile::errorKeyNotFound;
  const char mimeTypeSection[] = "mime types"; 
  const char defStr[] = "default";
  const char *cp = defStr;
  if (!defaultType) {
    cp = strrchr(_url, '.');
    if (cp)
      ++cp; // use character after '.'
  }

  if (cp)
    done = _ini.getValue(mimeTypeSection, cp, buffer, len);

  if (done == 1) {
    _client.print(contentType);
    _client.println(buffer);
  }
  return done;
}

// Return 1 to indicate all data sent. Use _stateData to store the file
// position
int8_t WwwServer::sendFile(char* buffer, int len)
{
  // Send file contents
  if (!_file.seek(_stateData)) {
    //_file.close();
    _client.println(); // send blank line after headers
    _client.print("Seek failed for ");
    _client.println(_url);
    return errorFileError;
  }
  
  if (_stateData == 0) {
    _client.print("Content-Length: ");
    _client.println(_file.size(), DEC);
    _client.println(); // send blank line after headers
  }


  int bytesRead = _file.read(buffer, len);
  _client.write((const uint8_t*)buffer, bytesRead);
  _stateData += bytesRead;
  if (!_file.available()) {
    //_file.close();
    return 1;
  }

  //file.close();
  return 0; // come back to send some more
}

void WwwServer::printHtmlPageHeader(const char* title)
{
  _client.print(contentType); _client.println(textHtml);
  _client.println(); // send blank line after headers
  _client.print(htmlToTitle);
  _client.print(title);
  _client.print(titleToH1);
  _client.print(title);
  _client.println(closeH1);
}

void WwwServer::printHtmlPageFooter(void)
{
  _client.println(closeBodyHtml);
}

void WwwServer::sendDirectoryListingHeader(void)
{
  printHtmlPageHeader(_url);
  _client.println("<p>");
  if (strcmp(_url, "/"))
    _client.println("<a href=\"..\">..</a><br />");
  _file.rewindDirectory();
}

int8_t WwwServer::sendDirectoryListingBody(char *buffer, int len)
{
  File f = _file.openNextFile(FILE_READ);
  if (f) {
    char *p = strncpy(buffer, f.name(), len);
    buffer[len-1] = '\0';
    while (*p) {
      *p = tolower(*p);
      ++p;
    }
    _client.print("<a href=\"");
    _client.print(buffer);
    if (f.isDirectory())
      _client.print('/');
    _client.print("\">");
    _client.print(buffer);
    if (f.isDirectory())
      _client.print('/');
    _client.println("</a><br />");
    f.close();
    return 0; // Not finished
  }
  else
    return 1;
}

void WwwServer::sendDirectoryListingFooter(void)
{
  _client.println("</p><hr />");
  printHtmlPageFooter();
}

// Send the web server status
void WwwServer::sendStatus(void)
{
  printHtmlPageHeader("Web server status");
  _client.print("<p>Total requests: ");
  _client.print(_stats.requestCount, DEC);
  _client.print("<br />\nWorst case request time: ");
  _client.print(_stats.requestTimeWorstCase, DEC);
  _client.print("uS<br />\nWorst case task time: ");
  _client.print(_stats.taskTimeWorstCase, DEC);
  _client.println("uS<br />\nWorst case task state: ");
  _client.print(_stats.taskWorstCaseState, DEC);
  _client.println("</p>");
  printHtmlPageFooter();
}

// For cases when no error document exists make one on demand
void WwwServer::sendError(const char *s)
{
  printHtmlPageHeader(responseText[_statusCode]);
  if (_url[0]) {
    _client.print("<p>");
    _client.print(_url);
    _client.println("</p>");
  }
  if (s && *s) {
    _client.print("<p>");
    _client.print(s);
    _client.println("</p>");
  }
  printHtmlPageFooter();
}

int8_t WwwServer::getState(void) const
{
  return _state;
}

const WwwServer::stats_t* WwwServer::getStats(void)
{
  return &_stats;
}

void WwwServer::updateStats(unsigned long startMicros, int8_t initialState)
{
  unsigned long endMicros = micros();
  unsigned long duration = endMicros - startMicros;
  
  if (endMicros < startMicros)
    // rollover!
    duration = (ULONG_MAX - startMicros) + 1 + endMicros;
  
  if (duration > _stats.taskTimeWorstCase) {
    _stats.taskTimeWorstCase = duration;
    _stats.taskWorstCaseState = initialState;
  }

  if (initialState == stateNoClient)
    _stats.requestStarted = startMicros;
  else
    if (initialState == stateDisconnecting) {
      _stats.requestCount += 1;
      duration = endMicros - _stats.requestStarted;
      if (endMicros < _stats.requestStarted) 
	// rollover!
	duration = (ULONG_MAX - _stats.requestStarted) + 1 + endMicros;

      if (duration > _stats.requestTimeWorstCase)
	_stats.requestTimeWorstCase = duration;
    }
}


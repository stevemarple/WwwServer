WwwServer is an Arduino HTTP library for making files on the SD card
available over HTTP.

In order to avoid interfering with time-critical code the server
operates on small chunks of work. All memory allocation is static,
with the working buffer passed to the library from user code when
processRequest() is called. The user is free to use the buffer between
calls to processRequest() as all state information is held internally
by the class.

The IniFile library is used to configure the server. Standard file
access by GET is implemented, as is making selected files and
directories inaccessible (403 Forbidden). CGI access methods, and
POST, PUT and DELETE methods are planned.

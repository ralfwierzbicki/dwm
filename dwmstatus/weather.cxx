/**
 * This snippet retrieves the Weather Forecast from Yahoo! Weather
 * and displays the current conditions in Dublin on the console. 
 * This can be easily adapted to any city by changing the "p"
 * value in the "yahoo_weather" URI.
 *
 * The aim of this snippet is to illustrate the use of libcurl
 * to handle a HTTP request, and of libxml2 to parse the resulting
 * XML output.
 *
 * This example uses libcurl and libxml2 directly.  Ideally, we would
 * use C++ wrappers such as curlpp and xmlccwrap, but I didn't want
 * to drag too many dependencies for such a simple snippet.
 *
 * libcurl header files can be easily installed on Ubuntu with
 * the following command:
 *
 * sudo apt-get install libcurl4-openssl-dev
 * 
 * libxml2 header files can be installed with:
 * 
 * sudo apt-get install libxml2-dev
 *
 * To compile:
 * g+ + -Wall -I/usr/include/libxml2 weather.cpp -o weather -lcurl -lxml2
 */
 
#include <iostream>
#include <string>
#include <curl/curl.h>
#include <string.h> 
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
 
 
 
// In Dublin fair city.
const std::string yahoo_weather = "http://weather.yahooapis.com/forecastrss?w=4125&u=c";
// XPath for retrieving the current weather conditions.
const std::string condition_xpath = "/rss/channel/item/yweather:condition";
// Namespace prefix.
const std::string yweather_prefix = "yweather";
// Namespace URI.
const std::string yweather_namespace = "http://xml.weather.yahoo.com/ns/rss/1.0";
 
// write_data is a "callback" function called by libcurl when it receives
// the content.  This callback function is defined by using the option
// CURLOPT_WRITEFUNCTION when calling curl_easy_setopt.
size_t write_data(char* data, size_t size, size_t nmemb, std::string* buffer) {
  if (buffer != NULL) {
    buffer->append(data, size*nmemb);
    return size*nmemb;
  }
  return 0;
}
 
// This function does the HTTP request and puts the response in a std::string.
std::string retrieveContent() {
  CURL *curl;
  std::string content = "";
   
  curl = curl_easy_init();
  if (curl) {
    // URI to fetch.
    curl_easy_setopt(curl, CURLOPT_URL, yahoo_weather.c_str());
    // Define callback function that will be called...
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &write_data);
    // and pass the object this function will stuff the content in.
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &content);
     
    // HTTP request is done here. curl calls the write_data function
    // and passed a pointer to the "content" string.
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
  }
   
  return content;
}
 
// libxml2 API is quite raw: you have to loop through the attributes to retrieve
// the ones you are actually looking for.
xmlAttrPtr findAttribute(xmlNodePtr node, std::string attributeName) {
  for (xmlAttrPtr attr = node->properties; attr != NULL; attr = attr->next) {
    if (!xmlStrcmp(attr->name, (xmlChar*)attributeName.c_str())) {
      return attr;
    }
  }
  return NULL;
}
 
// Parses a string, find the condition tag with XPath, and 
// retrieves the "text" and "temp" attributes of this tag.
char *parseAndDisplayWeather(std::string content) {
  char *ret;
  xmlDocPtr doc;
  xmlNode* root = NULL;
  xmlNode* conditions = NULL;
  xmlXPathContextPtr context;
  xmlXPathObjectPtr xpathObj;
   
  // Parse the document
  doc = xmlReadMemory(content.c_str(), content.length(), "_.xml", NULL, 0);
  root = xmlDocGetRootElement(doc);
   
  context = xmlXPathNewContext(doc);
  // Register the yweather XML namespace.
  xmlXPathRegisterNs(context, (xmlChar*)yweather_prefix.c_str(), (xmlChar*)yweather_namespace.c_str());
   
  // Evaluate the XPath Expression: this returns the condition node.
  xpathObj = xmlXPathEvalExpression((xmlChar*)condition_xpath.c_str(), context);
  // There should only be one, so take the first one.
  conditions = xpathObj->nodesetval->nodeTab[0];
  xmlAttrPtr text = findAttribute(conditions, "text");

  ret = (char *) malloc (4096 * sizeof(char));
  if (text) {
    sprintf (ret, "%s ", text->children->content);
  }
  xmlAttrPtr temp = findAttribute(conditions, "temp");
  if (temp) {
    strcat (ret, (char *) temp->children->content); 
 }
   
  // Cleanup
  xmlXPathFreeObject(xpathObj);
  xmlXPathFreeContext(context);
  xmlFreeDoc(doc);
  xmlCleanupParser();

  return ret;
}
 
 
char *weather () {
  char *ret = NULL;
  std::string content = retrieveContent();
   
  // Uncomment if you want to print out the content of the response.
  //std::cout << content <<std::endl;
   
  if (content.length() > 0) {
    ret = parseAndDisplayWeather(content);
  }
  return ret;   
}

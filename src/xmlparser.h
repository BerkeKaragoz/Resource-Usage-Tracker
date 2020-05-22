#ifndef XMLPARSER_H_
#define XMLPARSER_H_

#include <libxml/xinclude.h>

#include "resource_usage_tracker.h"

xmlNodePtr findNodeByName(xmlNodePtr rootnode, const xmlChar *nodename);

int readXmlTree(rut_config_ty *config, char *file);

#endif /* XMLPARSER_H_ */
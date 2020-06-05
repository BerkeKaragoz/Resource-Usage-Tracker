#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/xmlIO.h>
#include <libxml/xinclude.h>
#include <libxml/tree.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "xmlparser.h"
#include "resource_usage_tracker.h"
#include "berkelib/utils_.h"


xmlNodePtr findNodeByName(xmlNodePtr rootnode, const xmlChar *nodename)
{
	xmlNodePtr node = rootnode;
	if(node == NULL){
		return NULL;
	}

	while(node != NULL){

		if(!xmlStrcmp(node->name, nodename)){
			return node;
		}
		else if (node->children != NULL) {
			xmlNodePtr intNode =  findNodeByName(node->children, nodename);
			if(intNode != NULL) {
				return intNode;
			}
		}
		node = node->next;
	}
	return NULL;
}


int readXmlTree(rut_config_ty *config, char *file){

	xmlDoc *doc = NULL;
	xmlNode *root_element = NULL;
	xmlNode *current_node = NULL;
	xmlNode *temp_node = NULL;
	struct _xmlAttr *temp_attribute;

	char *str = NULL;

	gsize out_len;

	g_fprintf(STD, "(XML) Reading file: %s\n", file);

	//parse the file and get the DOM 
	doc = xmlReadFile(file, NULL, 0);

	if (doc == NULL){
		g_fprintf(STD, "(XML) Parsing failed!\n");
		return 1;
	}
	
	root_element = xmlDocGetRootElement(doc);

#ifdef DEBUG_RUT
	g_fprintf(STD, "(XML) Root: %s\n", root_element->name);
#endif

	if(!root_element){
		xmlFreeDoc(doc);
		xmlCleanupParser();
		g_fprintf(STD, "(XML) Parsing failed!\n");
		return -1;
	}

	current_node = findNodeByName(root_element, (xmlChar*) "rut");
	current_node = findNodeByName(current_node, (xmlChar*) "config");
	current_node = findNodeByName(current_node, (xmlChar*) "timelimit");

	str_to_uint32( (gchar *) XML_GET_CONTENT(current_node->children), &config->timelimit_ms );
#ifdef DEBUG_RUT
	g_fprintf(STD, "(XML) Timelimit: %u\n", (*config).timelimit_ms);
#endif
	current_node = findNodeByName(current_node, (xmlChar*) "resources");

	uint16_t itemcount = 0;
	while(current_node != NULL){

		if(current_node->type == XML_TEXT_NODE){
			current_node = current_node->next;
			continue;
		}

		//fprintf(STD, "(XML): %s\n", curr_node->properties->children->name );
		current_node = findNodeByName(current_node, (xmlChar*) "item");

		gchar *temp_name = NULL;
		uint32_t temp_interval = UINT32_MAX;
		gfloat temp_alert = G_MAXFLOAT;
		temp_node = NULL;
		for(temp_attribute = current_node->properties ; temp_attribute != NULL; temp_attribute = temp_attribute->next){

			if(!g_strcmp0((char*)temp_attribute->name, "name")){
				
				temp_name = (gchar *) XML_GET_CONTENT(temp_attribute->children);
#ifdef DEBUG_RUT
				g_fprintf(STD, "(XML) Name: %s\n", temp_name );
#endif
				if (temp_attribute->next != NULL) temp_attribute = temp_attribute->next;
			}
			
			if(!g_strcmp0((char*)temp_attribute->name, "interval")){
				
				str_to_uint32(XML_GET_CONTENT(temp_attribute->children), &temp_interval);
#ifdef DEBUG_RUT
				g_fprintf(STD, "(XML) Interval: %u\n", temp_interval );
#endif
				if (temp_attribute->next != NULL) temp_attribute = temp_attribute->next; 
			}

			if(!g_strcmp0((char*)temp_attribute->name, "alert")){

				str_to_float(XML_GET_CONTENT(temp_attribute->children), &temp_alert);
#ifdef DEBUG_RUT
				g_fprintf(STD, "(XML) Alert: %f\n", temp_alert );
#endif
				if (temp_attribute->next != NULL) temp_attribute = temp_attribute->next; 
			}

			//Assigning the values from buffer taken because of the possibility of unordered attributes
			if(g_strcmp0( temp_name, NULL )){

				if(!g_strcmp0( temp_name, "cpu" )){

					(*config).cpu_interval = temp_interval;
					(*config).cpu_alert_usage = temp_alert;

				} else if(!g_strcmp0( temp_name, "ram" )){

					config->ram_interval = temp_interval;
					config->ram_alert_usage = temp_alert;

				} else if(!g_strcmp0( temp_name, "disk" )){

					config->disk_interval = temp_interval;
					config->disk_alert_usage = temp_alert;

				} else if(!g_strcmp0( temp_name, "network" )){

					config->netint_interval = temp_interval;
					config->netint_alert_usage = temp_alert;

				} else if(!g_strcmp0( temp_name, "filesystems" )){

					config->filesys_interval = temp_interval;
					config->filesys_alert_usage = temp_alert;

				} 

				temp_name = NULL;
				temp_interval = UINT32_MAX;
				temp_alert = G_MAXFLOAT;
			}

		}

		current_node = current_node->next;
	}
	

	return 0;
}
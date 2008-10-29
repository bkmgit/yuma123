/*  FILE: agt_val_parse.c

		
*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
11feb06      abb      begun
06aug08      abb      YANG redesign

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <math.h>

#include <xmlstring.h>
#include <xmlreader.h>

#ifndef _H_procdefs
#include  "procdefs.h"
#endif

#ifndef _H_agt
#include "agt.h"
#endif

#ifndef _H_agt_ses
#include "agt_ses.h"
#endif

#ifndef _H_agt_top
#include "agt_top.h"
#endif

#ifndef _H_agt_util
#include "agt_util.h"
#endif

#ifndef _H_agt_val_parse
#include "agt_val_parse.h"
#endif

#ifndef _H_agt_xml
#include "agt_xml.h"
#endif

#ifndef _H_b64
#include "b64.h"
#endif

#ifndef _H_cfg
#include "cfg.h"
#endif

#ifndef _H_def_reg
#include "def_reg.h"
#endif

#ifndef _H_dlq
#include "dlq.h"
#endif

#ifndef _H_log
#include "log.h"
#endif

#ifndef _H_ncx
#include "ncx.h"
#endif

#ifndef _H_ncxconst
#include "ncxconst.h"
#endif

#ifndef _H_ncxtypes
#include "ncxtypes.h"
#endif

#ifndef _H_obj
#include "obj.h"
#endif

#ifndef _H_status
#include  "status.h"
#endif

#ifndef _H_tk
#include "tk.h"
#endif

#ifndef _H_typ
#include "typ.h"
#endif

#ifndef _H_val
#include "val.h"
#endif

#ifndef _H_val_util
#include "val_util.h"
#endif

#ifndef _H_xmlns
#include "xmlns.h"
#endif

#ifndef _H_xml_util
#include "xml_util.h"
#endif

#ifndef _H_yangconst
#include "yangconst.h"
#endif

/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/

#ifdef DEBUG
#define AGT_VAL_PARSE_DEBUG 1
#endif


/* forward declaration for recursive calls */
static status_t 
    parse_btype_nc (ses_cb_t  *scb,
		    xml_msg_hdr_t *msg,
		    const obj_template_t *obj,
		    const xml_node_t *startnode,
		    ncx_data_class_t parentdc,
		    val_value_t  *retval);


/********************************************************************
* FUNCTION parse_error_subtree
* 
* Generate an error during parmset processing for an element
* Add rpc_err_rec_t structs to the msg->errQ
*
* INPUTS:
*   scb == session control block (NULL means call is a no-op)
*   msg = xml_msg_hdr_t from msg in progress  (NULL means call is a no-op)
*   startnode == parent start node to match on exit
*         If this is NULL then the reader will not be advanced
*   errnode == error node being processed
*         If this is NULL then the current node will be
*         used if it can be retrieved with no errors,
*         and the node has naming properties.
*   errcode == error status_t of initial internal error
*         This will be used to pick the error-tag and
*         default error message
*   errnodetyp == internal VAL_PCH node type used for error_parm
*   error_parm == pointer to attribute name or namespace name, etc.
*         used in the specific error in agt_rpcerr.c
*   intnodetyp == internal VAL_PCH node type used for intnode
*   intnode == internal VAL_PCH node used for error_path
* RETURNS:
*   status of the operation; only fatal errors will be returned
*********************************************************************/
static status_t 
    parse_error_subtree (ses_cb_t *scb,
			 xml_msg_hdr_t *msg,
			 const xml_node_t *startnode,
			 const xml_node_t *errnode,
			 status_t errcode,
			 ncx_node_t errnodetyp,			    
			 const void *error_parm,
			 ncx_node_t intnodetyp,
			 const void *intnode)
{
    status_t        res;

    res = NO_ERR;

    agt_record_error(scb, msg, NCX_LAYER_OPERATION, errcode, 
		     errnode, errnodetyp, error_parm, 
		     intnodetyp, intnode);

    if (scb && startnode) {
	res = agt_xml_skip_subtree(scb, startnode);
    }

    return res;

}  /* parse_error_subtree */


/********************************************************************
* FUNCTION parse_error_subtree_errinfo
* 
* Generate an error during parmset processing for an element
* Add rpc_err_rec_t structs to the msg->errQ
*
* INPUTS:
*   scb == session control block (NULL means call is a no-op)
*   msg = xml_msg_hdr_t from msg in progress  (NULL means call is a no-op)
*   startnode == parent start node to match on exit
*         If this is NULL then the reader will not be advanced
*   errnode == error node being processed
*         If this is NULL then the current node will be
*         used if it can be retrieved with no errors,
*         and the node has naming properties.
*   errcode == error status_t of initial internal error
*         This will be used to pick the error-tag and
*         default error message
*   errnodetyp == internal VAL_PCH node type used for error_parm
*   error_parm == pointer to attribute name or namespace name, etc.
*         used in the specific error in agt_rpcerr.c
*   intnodetyp == internal VAL_PCH node type used for intnode
*   intnode == internal VAL_PCH node used for error_path
*   errinfo == errinfo struct to use for <rpc-error> details
*
* RETURNS:
*   status of the operation; only fatal errors will be returned
*********************************************************************/
static status_t 
    parse_error_subtree_errinfo (ses_cb_t *scb,
				 xml_msg_hdr_t *msg,
				 const xml_node_t *startnode,
				 const xml_node_t *errnode,
				 status_t errcode,
				 ncx_node_t errnodetyp,	    
				 const void *error_parm,
				 ncx_node_t intnodetyp,
				 const void *intnode,
				 const ncx_errinfo_t *errinfo)
{
    status_t        res;

    res = NO_ERR;

    agt_record_error_errinfo(scb, msg, NCX_LAYER_OPERATION, errcode, 
			     errnode, errnodetyp, error_parm, 
			     intnodetyp, intnode, errinfo);

    if (scb && startnode) {
	res = agt_xml_skip_subtree(scb, startnode);
    }

    return res;

}  /* parse_error_subtree_errinfo */


/********************************************************************
 * FUNCTION get_xml_node
 * 
 * Get the next (or maybe current) XML node in the reader stream
 * This hack needed because xmlTextReader cannot look ahead or
 * back up during processing.
 * 
 * The YANG leaf-list is really a collection of sibling nodes
 * and there is no way to tell where it ends without reading
 * past the end of it.
 *
 * This hack relies on the fact that a top-levelleaf-list could
 * never show up in a real NETCONF PDU
 *
 * INPUTS:
 *   scb == session control block
 *   msg == xml_msg_hdr_t in progress (NULL means don't record errors)
 *   xmlnode == xml_node_t to fill in
 *   usens == TRUE if regular NS checking mode
 *         == FALSE if _nons version should be used
 *
 * OUTPUTS:
 *   *xmlnode filled in
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t 
    get_xml_node (ses_cb_t  *scb,
		  xml_msg_hdr_t *msg,
		  xml_node_t *xmlnode,
		  boolean usens)

{
    status_t   res;

    if (scb->xmladvance) {
	/* get a new node */
	if (usens) {
	    res = agt_xml_consume_node(scb, xmlnode,
				       NCX_LAYER_OPERATION, 
				       msg);
	} else {
	    res = agt_xml_consume_node_nons(scb, xmlnode,
					    NCX_LAYER_OPERATION, 
					    msg);
	}
    } else {
	/* get current node, not a new node */
	if (usens) {
	    res = agt_xml_consume_node_noadv(scb, xmlnode,
					     NCX_LAYER_OPERATION, 
					     msg);
	} else {
	    res = agt_xml_consume_node_nons_noadv(scb, xmlnode,
						  NCX_LAYER_OPERATION, 
						  msg);
	}
    }
    scb->xmladvance = TRUE;
    return res;

}   /* get_xml_node */


/********************************************************************
 * FUNCTION get_editop
 * 
 * Check the node for operation="foo" attribute
 * and convert its value to an op_editop_t enum
 *
 * INPUTS:
 *   node == xml_node_t to check
 * RETURNS:
 *   editop == will be OP_EDITOP_NONE if explicitly set,
 *             not-present, or error
 *********************************************************************/
static op_editop_t
    get_editop (const xml_node_t  *node)
{
    const xml_attr_t  *attr;

    attr = xml_find_ro_attr(node, xmlns_nc_id(), NC_OPERATION_ATTR_NAME);
    if (!attr) {
	return OP_EDITOP_NONE;
    }
    return op_editop_id(attr->attr_val);

} /* get_editop */


/********************************************************************
 * FUNCTION pick_dataclass
 * 
 * Pick the correct data class for this value node
 *
 * INPUTS:
 *   parentdc == parent data class
 *   obj == object template definition struct for the value node
 *
 * RETURNS:
 *   data class for this value node
 *********************************************************************/
static ncx_data_class_t
    pick_dataclass (ncx_data_class_t parentdc,
		    const obj_template_t *obj)
{
    boolean  ret, setflag;

    setflag = FALSE;
    ret = obj_get_config_flag2(obj, &setflag);

    if (setflag) {
	return (ret) ? NCX_DC_CONFIG : NCX_DC_STATE;
    } else {
	return parentdc;
    }
    /*NOTREACHED*/

} /* pick_dataclass */


/********************************************************************
 * FUNCTION parse_any_nc
 * 
 * Parse the XML input as an 'any' type 
 *
 * INPUTS:
 *   see parse_btype_nc parameter list
 * RETURNS:
 *   status
 *********************************************************************/
static status_t 
    parse_any_nc (ses_cb_t  *scb,
		  xml_msg_hdr_t *msg,
		  const xml_node_t *startnode,
		  ncx_data_class_t parentdc,
		  val_value_t  *retval)
{
    const xml_node_t        *errnode;
    val_value_t             *chval;
    status_t                 res, res2;
    boolean                  done, getstrend, errdone;
    xml_node_t               nextnode;

    /* init local vars */
    errnode = startnode;
    chval = NULL;
    res = NO_ERR;
    res2 = NO_ERR;
    done = FALSE;
    getstrend = FALSE;
    errdone = FALSE;
    xml_init_node(&nextnode);

    retval->dataclass = parentdc;

    /* make sure the startnode is correct */
    switch (startnode->nodetyp) {
    case XML_NT_START:
	/* do not set the object template yet, in case
	 * there is a <foo></foo> form of empty element
	 * or another start (child) node
	 */
	break;
    case XML_NT_EMPTY:
	/* treat this 'any' is an 'empty' data type  */
	val_init_from_template(retval, ncx_get_gen_empty());
	retval->v.bool = TRUE;
	retval->nsid = startnode->nsid;
	return NO_ERR;
    default:
	res = ERR_NCX_WRONG_NODETYP;
    }

    if (res == NO_ERR) {
	/* at this point have either a simple type or a complex type
	 * get the next node which could be any type 
	 */
	res = get_xml_node(scb, msg, &nextnode, FALSE);
	if (res != NO_ERR) {
	    errdone = TRUE;
	}
    }

    if (res == NO_ERR) {

#ifdef AGT_VAL_PARSE_DEBUG
	log_debug3("\nparse_any: expecting any node type");
	if (LOGDEBUG3) {
	    xml_dump_node(&nextnode);
	}
#endif

	/* decide the base type from the child node type */
	switch (nextnode.nodetyp) {
	case XML_NT_START:
	case XML_NT_EMPTY:
	    /* A nested start or empty element means the parent is
	     * treated as a 'container' data type
	     */
	    val_init_from_template(retval, ncx_get_gen_container());
	    retval->nsid = startnode->nsid;
	    break;
	case XML_NT_STRING:
	    /* treat this string child node as the string
	     * content for the parent node
	     */
	    val_init_from_template(retval, ncx_get_gen_string());
	    retval->nsid = startnode->nsid;
	    retval->v.str = xml_strdup(nextnode.simval);
	    res = (retval->v.str) ? NO_ERR : ERR_INTERNAL_MEM;
	    getstrend = TRUE;
	    break;
	case XML_NT_END:
	    res = xml_endnode_match(startnode, &nextnode);
	    if (res == NO_ERR) {
		/* treat this start + end pair as an 'empty' data type */
		val_init_from_template(retval, ncx_get_gen_empty());
		retval->v.bool = TRUE;
		retval->nsid = startnode->nsid;
		return NO_ERR;
	    } else {
		errnode = &nextnode;
	    }
	    break;
	default:
	    res = SET_ERROR(ERR_INTERNAL_VAL);
	    errnode = &nextnode;
	}
    }

    /* check if processing a simple type as a string */
    if (getstrend) {
	/* need to get the endnode for startnode then exit */
	xml_clean_node(&nextnode);
	res2 = get_xml_node(scb, msg, &nextnode, FALSE);
	if (res2 == NO_ERR) {
#ifdef AGT_VAL_PARSE_DEBUG
	    log_debug3("\nparse_any: expecting end node for %s", 
		   startnode->qname);
	    if (LOGDEBUG3) {
		xml_dump_node(&nextnode);
	    }
#endif
	    res2 = xml_endnode_match(startnode, &nextnode);
	} else {
	    errdone = TRUE;
	}
	if (res2 != NO_ERR) {
	    errnode = &nextnode;
	}
    }

    /* check if there were any errors in the startnode */
    if (res != NO_ERR || res2 != NO_ERR) {
	if (!errdone) {
	    /* add rpc-error to msg->errQ */
	    (void)parse_error_subtree(scb, msg, startnode,
				      errnode, res, NCX_NT_NONE, 
				      NULL, NCX_NT_VAL, retval);
	}
	xml_clean_node(&nextnode);
	return (res==NO_ERR) ? res2 : res;
    }

    if (getstrend) {
	return NO_ERR;
    }

    /* if we get here, then the startnode is a container */
    while (!done) {
	/* At this point have a nested start node
	 *  Allocate a new val_value_t for the child value node 
	 */
	res = NO_ERR;
	chval = val_new_child_val(nextnode.nsid, nextnode.elname, 
				  TRUE, retval, get_editop(&nextnode));
	if (!chval) {
	    res = ERR_INTERNAL_MEM;
	}

	/* check any error setting up the child node */
	if (res == NO_ERR) {
	    val_add_child(chval, retval);

	    /* recurse through and get whatever nodes are present
	     * in the child node; call it an 'any' type
	     * make sure this function gets called again
	     * so the namespace errors can be ignored properly ;-)
	     *
	     * Cannot call this function directly or the
	     * XML attributes will not get processed
	     */
	    res = parse_btype_nc(scb, msg, 
				 ncx_get_gen_anyxml(), 
				 &nextnode, 
				 retval->dataclass, 
				 chval);
	    chval->res = res;
	    xml_clean_node(&nextnode);

	    if (res != NO_ERR) {
		errdone = TRUE;
		chval = NULL;
	    }
	}

	/* record any error, if not already done */
	if (res != NO_ERR) {
	    if (!errdone) {
		/* add rpc-error to msg->errQ */
		(void)parse_error_subtree(scb, msg, startnode,
					  errnode, res, 
					  NCX_NT_NONE, NULL, 
					  NCX_NT_VAL, retval);
	    }
	    xml_clean_node(&nextnode);
	    if (chval) {
		val_free_value(chval);
	    }
	    return res;
	}

	/* get the next node */
	res = get_xml_node(scb, msg, &nextnode, FALSE);
	if (res == NO_ERR) {
#ifdef AGT_VAL_PARSE_DEBUG
	    log_debug3("\nparse_any: expecting start, empty, or end node");
	    if (LOGDEBUG3) {
		xml_dump_node(&nextnode);
	    }
#endif
	    res = xml_endnode_match(startnode, &nextnode);
	    if (res == NO_ERR) {
		done = TRUE;
	    }	    
	} else {
	    /* error already recorded */
	    done = TRUE;
	}
    }

    xml_clean_node(&nextnode);
    return res;

} /* parse_any_nc */


/********************************************************************
 * FUNCTION parse_enum_nc
 * 
 * Parse the XML input as a 'enumeration' type 
 * e.g..
 *
 * <foo>fred</foo>
 * 
 * These NCX variants are no longer supported:
 *    <foo>11</foo>
 *    <foo>fred(11)</foo>
 *
 * INPUTS:
 *   see parse_btype_nc parameter list
 * RETURNS:
 *   status
 *********************************************************************/
static status_t 
    parse_enum_nc (ses_cb_t  *scb,
		   xml_msg_hdr_t *msg,
		   const obj_template_t *obj,
		   const xml_node_t *startnode,
		   ncx_data_class_t parentdc,
		   val_value_t  *retval)
{
    const xml_node_t  *errnode;
    const xmlChar     *badval;
    xml_node_t         valnode, endnode;
    status_t           res, res2, res3;
    boolean            errdone, empty, enddone;

    /* init local vars */
    xml_init_node(&valnode);
    xml_init_node(&endnode);
    errnode = startnode;
    errdone = FALSE;
    empty = FALSE;
    enddone = FALSE;
    badval = NULL;
    res = NO_ERR;
    res2 = NO_ERR;
    res3 = NO_ERR;

    val_init_from_template(retval, obj);
    retval->dataclass = pick_dataclass(parentdc, obj);

    if (retval->editop == OP_EDITOP_DELETE) {
	switch (startnode->nodetyp) {
	case XML_NT_EMPTY:
	    empty = TRUE;
	    break;
	case XML_NT_START:
	    break;
	default:
	    res = ERR_NCX_WRONG_NODETYP;
	    errnode = startnode;
	}
    } else {
	/* make sure the startnode is correct */
	res = xml_node_match(startnode, obj_get_nsid(obj), 
			     NULL, XML_NT_START); 
    }

    if (res == NO_ERR && !empty) {
	/* get the next node which should be a string node */
	res = get_xml_node(scb, msg, &valnode, TRUE);
	if (res != NO_ERR) {
	    errdone = TRUE;
	}
    }

    if (res == NO_ERR && !empty) {
#ifdef AGT_VAL_PARSE_DEBUG
	log_debug3("\nparse_enum: expecting string node");
	if (LOGDEBUG3) {
	    xml_dump_node(&valnode);
	}
#endif

	/* validate the node type and enum string or number content */
	switch (valnode.nodetyp) {
	case XML_NT_START:
	    res = ERR_NCX_WRONG_NODETYP_CPX;
	    errnode = &valnode;
	    break;
	case XML_NT_EMPTY:
	    res = ERR_NCX_WRONG_NODETYP_CPX;
	    errnode = &valnode;
	    enddone = TRUE;
	    break;
	case XML_NT_STRING:
	    if (val_all_whitespace(valnode.simval) &&
		retval->editop == OP_EDITOP_DELETE) {
		res = NO_ERR;
	    } else {
		/* get the non-whitespace string here */
		res = val_enum_ok(obj_get_ctypdef(obj), 
				  valnode.simval, 
				  &retval->v.enu.val, 
				  &retval->v.enu.name);
	    }
	    if (res == NO_ERR) {
		/* record the value even if there are errors after this */
		retval->btyp = NCX_BT_ENUM;
	    } else {
		badval = valnode.simval;
	    }
	    break;
	case XML_NT_END:
	    enddone = TRUE;
	    if (retval->editop == OP_EDITOP_DELETE) {
		retval->btyp = NCX_BT_ENUM;
		/* leave value empty, OK for leaf */
	    } else {
		res = ERR_NCX_INVALID_VALUE;
		errnode = &valnode;
	    }
	    break;
	default:
	    res = SET_ERROR(ERR_NCX_WRONG_NODETYP);
	    errnode = &valnode;
	    enddone = TRUE;
	}

	/* get the matching end node for startnode */
	if (!enddone) {
	    res2 = get_xml_node(scb, msg, &endnode, TRUE);
	    if (res2 == NO_ERR) {
#ifdef AGT_VAL_PARSE_DEBUG
		log_debug3("\nparse_enum: expecting end for %s", 
			   startnode->qname);
		if (LOGDEBUG3) {
		    xml_dump_node(&endnode);
		}
#endif
		res2 = xml_endnode_match(startnode, &endnode);
		if (res2 != NO_ERR) {
		    errnode = &endnode;
		}
	    } else {
		errdone = TRUE;
	    }
	}
    }

    /* check if this is a list index, add it now so
     * error-path info will be as complete as possible
     */
    if (obj_is_key(retval->obj)) {
	res3 = val_gen_key_entry(retval);
    }

    if (res == NO_ERR) {
	res = res2;
    }

    if (res == NO_ERR) {
	res = res3;
    }

    /* check if any errors; record the first error */
    if ((res != NO_ERR) && !errdone) {
	/* add rpc-error to msg->errQ */
	(void)parse_error_subtree(scb, msg, startnode,
				  errnode, res, NCX_NT_STRING, 
				  badval, NCX_NT_VAL, retval);
    }

    xml_clean_node(&valnode);
    xml_clean_node(&endnode);
    return res;

} /* parse_enum_nc */


/********************************************************************
 * FUNCTION parse_empty_nc
 * For NCX_BT_EMPTY
 *
 * Parse the XML input as an 'empty' or 'boolean' type 
 * e.g.:
 *
 *  <foo/>
 * <foo></foo>
 * <foo>   </foo>
 *
 *
 * INPUTS:
 *   see parse_btype_nc parameter list
 * RETURNS:
 *   status
 *********************************************************************/
static status_t 
    parse_empty_nc (ses_cb_t  *scb,
		    xml_msg_hdr_t *msg,
		    const obj_template_t *obj,
		    const xml_node_t *startnode,
		    ncx_data_class_t parentdc,
		    val_value_t  *retval)
{
    const xml_node_t *errnode;
    xml_node_t        endnode;
    status_t          res;
    boolean           errdone;

    /* init local vars */
    xml_init_node(&endnode);
    errnode = startnode;
    errdone = FALSE;

    val_init_from_template(retval, obj);
    retval->dataclass = pick_dataclass(parentdc, obj);

    /* validate the node type and enum string or number content */
    switch (startnode->nodetyp) {
    case XML_NT_EMPTY:
	res = xml_node_match(startnode, obj_get_nsid(obj),
	     NULL, XML_NT_NONE);
	break;
    case XML_NT_START:
	res = xml_node_match(startnode, obj_get_nsid(obj),
	     NULL, XML_NT_NONE);
	if (res == NO_ERR) {
	    res = get_xml_node(scb, msg, &endnode, TRUE);
	    if (res != NO_ERR) {
		errdone = TRUE;
	    } else {
#ifdef AGT_VAL_PARSE_DEBUG
		log_debug3("\nparse_empty: expecting end for %s", 
		       startnode->qname);
		if (LOGDEBUG3) {
		    xml_dump_node(&endnode);
		}
#endif
		res = xml_endnode_match(startnode, &endnode);
		if (res != NO_ERR) {
		    if (endnode.nodetyp != XML_NT_STRING ||
			!xml_isspace_str(endnode.simval)) {
			errnode = &endnode;
			res = ERR_NCX_WRONG_NODETYP;
		    } else {
			/* that was an empty string -- try again */
			xml_clean_node(&endnode);
			res = get_xml_node(scb, msg, &endnode, TRUE);
			if (res == NO_ERR) {
#ifdef AGT_VAL_PARSE_DEBUG
			    log_debug3("\nparse_empty: expecting end for %s", 
				   startnode->qname);
			    if (LOGDEBUG3) {
				xml_dump_node(&endnode);
			    }
#endif
			    res = xml_endnode_match(startnode, &endnode);
			    if (res != NO_ERR) {
				errnode = &endnode;
			    }
			} else {
			    errdone = TRUE;
			}
		    }
		}
	    }
	}
	break;
    default:
	res = ERR_NCX_WRONG_NODETYP;
    }

    /* record the value if no errors */
    if (res == NO_ERR) {
	retval->v.bool = TRUE;
    } else if (!errdone) {
	/* add rpc-error to msg->errQ */
	(void)parse_error_subtree(scb, msg, startnode,
				  errnode, res, 
				  NCX_NT_NONE, NULL, 
				  NCX_NT_VAL, retval);
    }

    xml_clean_node(&endnode);
    return res;

} /* parse_empty_nc */


/********************************************************************
 * FUNCTION parse_boolean_nc
 * 
 * Parse the XML input as a 'boolean' type 
 * e.g..
 *
 * <foo>true</foo>
 * <foo>false</foo>
 * <foo>1</foo>
 * <foo>0</foo>
 *
 * INPUTS:
 *   see parse_btype_nc parameter list
 * RETURNS:
 *   status
 *********************************************************************/
static status_t 
    parse_boolean_nc (ses_cb_t  *scb,
		      xml_msg_hdr_t *msg,
		      const obj_template_t *obj,
		      const xml_node_t *startnode,
		      ncx_data_class_t parentdc,
		      val_value_t  *retval)
{
    const xml_node_t  *errnode;
    const xmlChar     *badval;
    xml_node_t         valnode, endnode;
    status_t           res, res2, res3;
    boolean            errdone;

    /* init local vars */
    xml_init_node(&valnode);
    xml_init_node(&endnode);
    errnode = startnode;
    errdone = FALSE;
    badval = NULL;
    res2 = NO_ERR;
    res3 = NO_ERR;

    val_init_from_template(retval, obj);
    retval->dataclass = pick_dataclass(parentdc, obj);

    /* make sure the startnode is correct */
    res = xml_node_match(startnode, obj_get_nsid(obj), 
			 NULL, XML_NT_START); 
    if (res == NO_ERR) {
	/* get the next node which should be a string node */
	res = get_xml_node(scb, msg, &valnode, TRUE);
	if (res != NO_ERR) {
	    errdone = TRUE;
	}
    }

    if (res == NO_ERR) {
#ifdef AGT_VAL_PARSE_DEBUG
	log_debug3("\nparse_boolean: expecting string node.");
	if (LOGDEBUG3) {
	    xml_dump_node(&valnode);
	}
#endif

	/* validate the node type and enum string or number content */
	switch (valnode.nodetyp) {
	case XML_NT_START:
	    res = ERR_NCX_WRONG_NODETYP_CPX;
	    errnode = &valnode;
	    break;
	case XML_NT_STRING:
	    /* get the non-whitespace string here */
	    if (ncx_is_true(valnode.simval)) {
		retval->v.bool = TRUE;
	    } else if (ncx_is_false(valnode.simval)) {
		retval->v.bool = FALSE;
	    } else {
		res = ERR_NCX_INVALID_VALUE;
		badval = valnode.simval;
	    }
	    break;
	default:
	    res = ERR_NCX_WRONG_NODETYP;
	    errnode = &valnode;
	}

	/* get the matching end node for startnode */
	res2 = get_xml_node(scb, msg, &endnode, TRUE);
	if (res2 == NO_ERR) {
#ifdef AGT_VAL_PARSE_DEBUG
	    log_debug3("\nparse_boolean: expecting end for %s", 
		       startnode->qname);
	    if (LOGDEBUG3) {
		xml_dump_node(&endnode);
	    }
#endif
	    res2 = xml_endnode_match(startnode, &endnode);
	    if (res2 != NO_ERR) {
		errnode = &endnode;
	    }
	} else {
	    errdone = TRUE;
	}
    }

    /* check if this is a list index, add it now so
     * error-path info will be as complete as possible
     */
    if (obj_is_key(retval->obj)) {
	res3 = val_gen_key_entry(retval);
    }

    if (res == NO_ERR) {
	res = res2;
    }

    if (res == NO_ERR) {
	res = res3;
    }

    /* check if any errors; record the first error */
    if ((res != NO_ERR) && !errdone) {
	/* add rpc-error to msg->errQ */
	(void)parse_error_subtree(scb, msg, startnode,
				  errnode, res, 
				  NCX_NT_STRING, badval, 
				  NCX_NT_VAL, retval);
    }

    xml_clean_node(&valnode);
    xml_clean_node(&endnode);
    return res;

} /* parse_boolean_nc */


/********************************************************************
* FUNCTION parse_num_nc
* 
* Parse the XML input as a numeric data type 
*
* INPUTS:
*     scb == session control block
*            Input is read from scb->reader.
*     msg == incoming RPC message
*            Errors are appended to msg->errQ
*     obj == object template for this number type
*     btyp == base type of the expected ordinal number type
*     startnode == top node of the parameter to be parsed
*            Parser function will attempt to consume all the
*            nodes until the matching endnode is reached
*     parentdc == data class of the parent node
*     retval ==  val_value_t that should get the results of the parsing
*     
* OUTPUTS:
*    *retval will be filled in
*    msg->errQ may be appended with new errors or warnings
*
* RETURNS:
*   status
*********************************************************************/
static status_t 
    parse_num_nc (ses_cb_t  *scb,
		  xml_msg_hdr_t *msg,
		  const obj_template_t *obj,
		  ncx_btype_t  btyp,
		  const xml_node_t *startnode,
		  ncx_data_class_t parentdc,
		  val_value_t  *retval)
{
    const xml_node_t    *errnode;
    const xmlChar       *badval;
    const ncx_errinfo_t *errinfo;
    xml_node_t           valnode, endnode;
    status_t             res, res2, res3;
    boolean              errdone;

    /* init local vars */
    xml_init_node(&valnode);
    xml_init_node(&endnode);
    badval = NULL;
    errnode = startnode;
    errdone = FALSE;
    errinfo = NULL;
    res2 = NO_ERR;
    res3 = NO_ERR;

    val_init_from_template(retval, obj);
    retval->dataclass = pick_dataclass(parentdc, obj);
    
    /* make sure the startnode is correct */
    res = xml_node_match(startnode, obj_get_nsid(obj),
			 NULL, XML_NT_START); 
    if (res == NO_ERR) {
	/* get the next node which should be a string node */
	res = get_xml_node(scb, msg, &valnode, TRUE);
	if (res != NO_ERR) {
	    errdone = TRUE;
	}
    }

    if (res != NO_ERR) {
	/* fatal error */
	if (!errdone) {
	    (void)parse_error_subtree(scb, msg, startnode,
				      errnode, res, NCX_NT_NONE, 
				      NULL, NCX_NT_VAL, retval);
	}
	xml_clean_node(&valnode);
	return res;
    }

#ifdef AGT_VAL_PARSE_DEBUG
    log_debug3("\nparse_num: expecting string node.");
    if (LOGDEBUG3) {
	xml_dump_node(&valnode);
    }
#endif

    /* validate the number content */
    switch (valnode.nodetyp) {
    case XML_NT_START:
	res = ERR_NCX_WRONG_NODETYP_CPX;
	errnode = &valnode;
	break;
    case XML_NT_STRING:
	/* get the non-whitespace string here */
	res = ncx_decode_num(valnode.simval, btyp, &retval->v.num);
	if (res == NO_ERR) {
	    res = val_range_ok_errinfo(obj_get_ctypdef(obj), btyp, 
				       &retval->v.num, &errinfo);
	}
	if (res != NO_ERR) {
	    badval = valnode.simval;
	}
	break;
    default:
	res = ERR_NCX_WRONG_NODETYP;
	errnode = &valnode;
    }

    /* get the matching end node for startnode */
    res2 = get_xml_node(scb, msg, &endnode, TRUE);
    if (res2 == NO_ERR) {
#ifdef AGT_VAL_PARSE_DEBUG
	log_debug3("\nparse_num: expecting end for %s", startnode->qname);
	if (LOGDEBUG3) {
	    xml_dump_node(&endnode);
	}
#endif
	res2 = xml_endnode_match(startnode, &endnode);
	if (res2 != NO_ERR) {
	    errnode = &endnode;
	}
    } else {
	errdone = TRUE;
    }

    /* check if this is a list index, add it now so
     * error-path info will be as complete as possible
     */
    if (obj_is_key(retval->obj)) {
	res3 = val_gen_key_entry(retval);
    }

    if (res == NO_ERR) {
	res = res2;
    }

    if (res == NO_ERR) {
	res = res3;
    }

    /* check if any errors; record the first error */
    if ((res != NO_ERR) && !errdone) {
	/* add rpc-error to msg->errQ */
	(void)parse_error_subtree_errinfo(scb, msg, startnode,
					  errnode, res, NCX_NT_STRING, 
					  badval, NCX_NT_VAL, retval, errinfo);
    }

    xml_clean_node(&valnode);
    xml_clean_node(&endnode);
    return res;

} /* parse_num_nc */


/********************************************************************
* FUNCTION parse_string_nc
* 
* Parse the XML input as a numeric data type 
*
* INPUTS:
*     scb == session control block
*            Input is read from scb->reader.
*     msg == incoming RPC message
*            Errors are appended to msg->errQ
*     obj == object template for this string type
*     btyp == base type of the expected ordinal number type
*     startnode == top node of the parameter to be parsed
*            Parser function will attempt to consume all the
*            nodes until the matching endnode is reached
*     retval ==  val_value_t that should get the results of the parsing
*     
* OUTPUTS:
*    *retval will be filled in
*    msg->errQ may be appended with new errors or warnings
*
* RETURNS:
*   status
*********************************************************************/
static status_t 
    parse_string_nc (ses_cb_t  *scb,
		     xml_msg_hdr_t *msg,
		     const obj_template_t *obj,
		     ncx_btype_t  btyp,
		     const xml_node_t *startnode,
		     ncx_data_class_t parentdc,
		     val_value_t  *retval)
{
    const xml_node_t     *errnode;
    const xmlChar        *badval;
    const typ_template_t *listtyp;
    const ncx_errinfo_t  *errinfo;
    xml_node_t            valnode, endnode;
    status_t              res, res2, res3;
    boolean               errdone, empty, allow_delete_all;
    ncx_btype_t           listbtyp;

    /* init local vars */
    xml_init_node(&valnode);
    xml_init_node(&endnode);
    errnode = startnode;
    badval = NULL;
    errdone = FALSE;
    empty = FALSE;
    errinfo = NULL;
    res2 = NO_ERR;
    res3 = NO_ERR;

    allow_delete_all = FALSE;

    val_init_from_template(retval, obj);
    retval->dataclass = pick_dataclass(parentdc, obj);

    /* make sure the startnode is correct */
    res = xml_node_match(startnode, obj_get_nsid(obj),
			 NULL, XML_NT_START); 
    if (res != NO_ERR) {
	res = xml_node_match(startnode, obj_get_nsid(obj),
			     NULL, XML_NT_EMPTY); 
	if (res == NO_ERR) {
	    empty = TRUE;
	}
    }

    /* get the value string node */
    if (res == NO_ERR && !empty) {
	/* get the next node which should be a string node */
	res = get_xml_node(scb, msg, &valnode, TRUE);
	if (res != NO_ERR) {
	    errdone = TRUE;
	} else if (valnode.nodetyp == XML_NT_END) {
	    empty = TRUE;
	}
    }
  
    /* check empty string corner case */
    if (empty) {
	if (retval->editop == OP_EDITOP_DELETE) {
	    if (obj->objtype == OBJ_TYP_LEAF) {
		res = NO_ERR;
	    } else if (allow_delete_all) {
		res = NO_ERR;
	    } else {
		res = ERR_NCX_MISSING_VAL_INST;
	    }
	} else if (btyp==NCX_BT_SLIST || btyp==NCX_BT_BITS) {
	    /* no need to check the empty list */  ;
	} else {
	    /* check the empty string */
	    res = val_string_ok_errinfo(obj_get_ctypdef(obj), 
					btyp, (const xmlChar *)"",
					&errinfo);
	    retval->v.str = xml_strdup((const xmlChar *)"");
	    if (!retval->v.str) {
		res = ERR_INTERNAL_MEM;
	    }
	}
    }

    if (res != NO_ERR) {
	if (!errdone) {
	    /* add rpc-error to msg->errQ */
	    (void)parse_error_subtree_errinfo(scb, msg, startnode,
					      errnode, res, 
					      NCX_NT_NONE, NULL, 
					      NCX_NT_VAL, retval, errinfo);
	}
	xml_clean_node(&valnode);
	return res;
    }

    if (empty) {
	return NO_ERR;
    }

#ifdef AGT_VAL_PARSE_DEBUG
    log_debug3("\nparse_string: expecting string node.");
    if (LOGDEBUG3) {
	xml_dump_node(&valnode);
    }
#endif

    /* validate the number content */
    switch (valnode.nodetyp) {
    case XML_NT_START:
	res = ERR_NCX_WRONG_NODETYP_CPX;
	errnode = &valnode;
	break;
    case XML_NT_STRING:
	if (btyp == NCX_BT_SLIST || btyp==NCX_BT_BITS) {
	    if (btyp==NCX_BT_SLIST) {
		/* get the list of strings, then check them */
		listtyp = typ_get_listtyp(obj_get_ctypdef(obj));
		listbtyp = typ_get_basetype(&listtyp->typdef);
	    } else if (btyp==NCX_BT_BITS) {
		listbtyp = NCX_BT_BITS;
	    }

	    res = ncx_set_list(listbtyp, valnode.simval, 
			       &retval->v.list);
	    if (res == NO_ERR) {
		if (btyp == NCX_BT_SLIST) {
		    res = ncx_finish_list(&listtyp->typdef, 
					  &retval->v.list);
		} else {
		    res = ncx_finish_list(obj_get_ctypdef(obj),
					  &retval->v.list);
		}
	    }

	    if (res == NO_ERR) {
		res = val_list_ok_errinfo(obj_get_ctypdef(obj), 
					  btyp, &retval->v.list,
					  &errinfo);
	    }
	} else {
	    /* check the non-whitespace string */
	    res = val_string_ok_errinfo(obj_get_ctypdef(obj), 
					btyp, valnode.simval, &errinfo);
	}

	if (res != NO_ERR) {
	    badval = valnode.simval;
	}

	/* record the value even if there are errors */
	switch (btyp) {
	case NCX_BT_BINARY:
	    if (valnode.simval) {
		/* result is going to be less than the encoded length */
		retval->v.binary.ustr = m__getMem(valnode.simlen);
		retval->v.binary.ubufflen = valnode.simlen;
		if (!retval->v.binary.ustr) {
		    res = ERR_INTERNAL_MEM;
		} else {
		    res = b64_decode(valnode.simval, valnode.simlen,
				     retval->v.binary.ustr, 
				     retval->v.binary.ubufflen,
				     &retval->v.binary.ustrlen);
		}
	    }
	    break;
	case NCX_BT_STRING:
	case NCX_BT_INSTANCE_ID:
	case NCX_BT_KEYREF:
	    if (valnode.simval) {
		retval->v.str = xml_strdup(valnode.simval);
		if (!retval->v.str) {
		    res = ERR_INTERNAL_MEM;
		}
	    }
	    break;
	case NCX_BT_SLIST:
	case NCX_BT_BITS:
	    break;   /* value already set */
	default:
	    res = SET_ERROR(ERR_INTERNAL_VAL);
	}
	break;
    default:
	res = ERR_NCX_WRONG_NODETYP;
	errnode = &valnode;
    }

    /* get the matching end node for startnode */
    res2 = get_xml_node(scb, msg, &endnode, TRUE);
    if (res2 == NO_ERR) {
#ifdef AGT_VAL_PARSE_DEBUG
	log_debug3("\nparse_string: expecting end for %s", 
		   startnode->qname);
	if (LOGDEBUG3) {
	    xml_dump_node(&endnode);
	}
#endif
	res2 = xml_endnode_match(startnode, &endnode);
	
    } else {
	errdone = TRUE;
    }

    /* check if this is a list index, add it now so
     * error-path info will be as complete as possible
     */
    if (obj_is_key(retval->obj)) {
	res3 = val_gen_key_entry(retval);
    }

    if (res == NO_ERR) {
	res = res2;
    }

    if (res == NO_ERR) {
	res = res3;
    }

    /* check if any errors; record the first error */
    if ((res != NO_ERR)	&& !errdone) {
	/* add rpc-error to msg->errQ */
	(void)parse_error_subtree_errinfo(scb, msg, startnode,
					  errnode, res, NCX_NT_STRING,
					  badval, NCX_NT_VAL, retval, errinfo);
    }

    xml_clean_node(&valnode);
    xml_clean_node(&endnode);
    return res;

} /* parse_string_nc */


/********************************************************************
 * FUNCTION parse_union_nc
 * 
 * Parse the XML input as a 'union' type 
 * e.g..
 *
 * <foo>fred</foo>
 * <foo>11</foo>
 *
 * INPUTS:
 *   see parse_btype_nc parameter list
 * RETURNS:
 *   status
 *********************************************************************/
static status_t 
    parse_union_nc (ses_cb_t  *scb,
		    xml_msg_hdr_t *msg,
		    const obj_template_t *obj,
		    const xml_node_t *startnode,
		    ncx_data_class_t parentdc,
		    val_value_t  *retval)
{
    const xml_node_t    *errnode;
    const xmlChar       *badval;
    const ncx_errinfo_t *errinfo;
    xml_node_t           valnode, endnode;
    status_t             res, res2, res3;
    boolean              errdone;

    /* init local vars */
    xml_init_node(&valnode);
    xml_init_node(&endnode);
    errnode = startnode;
    errdone = FALSE;
    badval = NULL;
    res2 = NO_ERR;
    res3 = NO_ERR;
    errinfo = NULL;

    val_init_from_template(retval, obj);
    retval->dataclass = pick_dataclass(parentdc, obj);

    /* make sure the startnode is correct */
    if (res == NO_ERR) {
	res = xml_node_match(startnode, obj_get_nsid(obj), 
			     NULL, XML_NT_START); 
    }
    if (res == NO_ERR) {
	/* get the next node which should be a string node */
	res = get_xml_node(scb, msg, &valnode, TRUE);
	if (res != NO_ERR) {
	    errdone = TRUE;
	}
    }

    if (res == NO_ERR) {
#ifdef AGT_VAL_PARSE_DEBUG
	log_debug3("\nparse_union: expecting string or number node.");
	if (LOGDEBUG3) {
	    xml_dump_node(&valnode);
	}
#endif

	/* validate the node type and union node content */
	switch (valnode.nodetyp) {
	case XML_NT_START:
	    res = ERR_NCX_WRONG_NODETYP_CPX;
	    errnode = &valnode;
	    break;
	case XML_NT_STRING:
	    /* get the non-whitespace string here */
	    res = val_union_ok_errinfo(obj_get_ctypdef(obj), 
				       valnode.simval, retval, &errinfo);
	    if (res != NO_ERR) {
		badval = valnode.simval;
	    }
	    break;
	default:
	    res = ERR_NCX_WRONG_NODETYP;
	    errnode = &valnode;
	}

	/* get the matching end node for startnode */
	res2 = get_xml_node(scb, msg, &endnode, TRUE);
	if (res2 == NO_ERR) {
#ifdef AGT_VAL_PARSE_DEBUG
	    log_debug3("\nparse_union: expecting end for %s", 
		       startnode->qname);
	    if (LOGDEBUG3) {
		xml_dump_node(&endnode);
	    }
#endif
	    res2 = xml_endnode_match(startnode, &endnode);
	    if (res2 != NO_ERR) {
		errnode = &endnode;
	    }
	} else {
	    errdone = TRUE;
	}
    }

    /* check if this is a list index, add it now so
     * error-path info will be as complete as possible
     */
    if (obj_is_key(retval->obj)) {
	res3 = val_gen_key_entry(retval);
    }

    if (res == NO_ERR) {
	res = res2;
    }

    if (res == NO_ERR) {
	res = res3;
    }

    /* check if any errors; record the first error */
    if ((res != NO_ERR) && !errdone) {
	/* add rpc-error to msg->errQ */
	(void)parse_error_subtree_errinfo(scb, msg, startnode,
					  errnode, res, 
					  NCX_NT_STRING, badval, 
					  NCX_NT_VAL, retval, errinfo);
    }

    xml_clean_node(&valnode);
    xml_clean_node(&endnode);
    return res;

} /* parse_union_nc */


/********************************************************************
 * FUNCTION parse_complex_nc
 * 
 * Parse the XML input as a complex type
 *
 * Handles the following base types:
 *   NCX_BT_CONTAINER
 *   NCX_BT_LIST
 *
 * E.g., container:
 *
 * <foo>
 *   <a>blah</a>
 *   <b>7</b>
 *   <c/>
 * </foo>
 *
 * In an instance document, containers and lists look 
 * the same.  The validation is different of course, but the
 * parsing is basically the same.
 *
 * INPUTS:
 *   see parse_btype_nc parameter list
 * RETURNS:
 *   status
 *********************************************************************/
static status_t 
    parse_complex_nc (ses_cb_t  *scb,
		      xml_msg_hdr_t *msg,
		      const obj_template_t *obj,
		      const xml_node_t *startnode,
		      ncx_data_class_t parentdc,
		      val_value_t  *retval)
{
    const xml_node_t     *errnode;
    const obj_template_t *chobj, *topchild, *curchild, *nextchobj;
    const agt_profile_t  *profile;
    val_value_t          *chval;
    xml_node_t            chnode;
    status_t              res, res2, retres;
    boolean               done, errdone, empty, xmlorder;
    ncx_btype_t           chbtyp;

    /* setup local vars */
    errnode = startnode;
    chobj = NULL;
    topchild = NULL;
    curchild = NULL;
    nextchobj = NULL;
    res = NO_ERR;
    res2 = NO_ERR;
    retres = NO_ERR;
    done = FALSE;
    errdone = FALSE;
    empty = FALSE;

    profile = agt_get_profile();
    if (!profile) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
    xmlorder = profile->agt_xmlorder;

    val_init_from_template(retval, obj);
    retval->dataclass = pick_dataclass(parentdc, obj);

    /* do not really need to validate the start node type
     * since it is probably a virtual container at the
     * very start, and after that, a node match must have
     * occurred to call this function recursively
     */
    switch (startnode->nodetyp) {
    case XML_NT_START:
	break;
    case XML_NT_EMPTY:
	empty = TRUE;
	break;
    case XML_NT_STRING:
    case XML_NT_END:
	res = ERR_NCX_WRONG_NODETYP_SIM;
	break;
    default:
	res = ERR_NCX_WRONG_NODETYP;
    }

    if (res == NO_ERR) {
	/* start setting up the return value */
	retval->editop = get_editop(startnode);

	/* setup the first child in the complex object
	 * Allowed be NULL in some cases so do not check
	 * the result yet
	 */
	chobj = obj_first_child(obj);
    } else {
	/* add rpc-error to msg->errQ */
	(void)parse_error_subtree(scb, msg, startnode,
				  errnode, res, 
				  NCX_NT_NONE, NULL, 
				  NCX_NT_VAL, retval);
	return res;
    }

    if (empty) {
	return NO_ERR;
    }

    xml_init_node(&chnode);

    /* go through each child node until the parent end node */
    while (!done) {
	/* init per-loop vars */
	res2 = NO_ERR;
	errdone = FALSE;
	empty = FALSE;
	chval = NULL;

	/* get the next node which should be a child or end node */
	res = get_xml_node(scb, msg, &chnode, TRUE);
	if (res != NO_ERR) {
	    errdone = TRUE;
	    done = TRUE;
	} else {
#ifdef AGT_VAL_PARSE_DEBUG
	    log_debug3("\nparse_complex: expecting start-child or end node.");
	    if (LOGDEBUG3) {
		xml_dump_node(&chnode);
	    }
#endif
	    /* validate the child member node type */
	    switch (chnode.nodetyp) {
	    case XML_NT_START:
	    case XML_NT_EMPTY:
		/* any namespace OK for now, check it in get_child_node */
		break;
	    case XML_NT_STRING:
		res = ERR_NCX_WRONG_NODETYP_SIM;
		errnode = &chnode;
		break;
	    case XML_NT_END:
		res = xml_endnode_match(startnode, &chnode);
		if (res != NO_ERR) {
		    errnode = &chnode;
		} else {
		    /* no error exit */
		    done = TRUE;
		    continue;
		}
		break;
	    default:
		res = ERR_NCX_WRONG_NODETYP;
		errnode = &chnode;
	    }
	}

	/* if we get here, there is a START or EMPTY node
	 * that could be a valid child node
	 *
	 * if xmlorder enforced then check if the 
	 * node is the correct child node
	 *
	 * if no xmlorder, then check for any valid child
	 */
	if (res==NO_ERR) {
	    res = obj_get_child_node(obj, chobj, &chnode, xmlorder,
				     &topchild, &curchild);
	    if (res != NO_ERR) {
		/* have no expected child element to match to this XML node
		 * if xmlorder == TRUE, then this could indicate an extra
		 * instance of the last child node
		 */
		res = ERR_NCX_EXTRA_NODE;
		errnode = &chnode;
	    }
	}

	/* try to setup a new child node */
	if (res == NO_ERR) {
	    /* save the child base type */
	    chbtyp = obj_get_basetype(curchild);

	    /* at this point, the 'curchild' template matches the
	     * 'chnode' namespace and name;
	     * Allocate a new val_value_t for the child value node
	     */
	    chval = val_new_child_val(obj_get_nsid(curchild),
				      obj_get_name(curchild), 
				      FALSE, retval, 
				      get_editop(&chnode));
	    if (!chval) {
		res = ERR_INTERNAL_MEM;
	    }
	}

	/* check any errors in setting up the child node */
	if (res != NO_ERR) {
	    /* try to skip just the child node sub-tree */
	    if (!errdone) {
		/* add rpc-error to msg->errQ */
		res2 = parse_error_subtree(scb, msg, &chnode,
					   errnode, res, NCX_NT_NONE, 
					   NULL, NCX_NT_VAL, retval);
	    }
	    xml_clean_node(&chnode);
	    if (chval) {
		val_free_value(chval);
		chval = NULL;
	    }
	    if (res == ERR_NCX_UNKNOWN_NS || res2 != NO_ERR) {
		/* skip child didn't work, now skip the entire value subtree */
		(void)agt_xml_skip_subtree(scb, startnode);
		return res;
	    } else {
		/* skip child worked, go on to next child, parse for errors */
		retres = res;
		continue;
	    }
	}

	/* recurse through and get whatever nodes are present
	 * in the child node
	 */
	val_add_child(chval, retval);
	res = parse_btype_nc(scb, msg, curchild,
			     &chnode, retval->dataclass, chval);
	chval->res = res;
	if (res != NO_ERR) {
	    retres = res;
	}

	/* setup the next child unless the current child
	 * is a list
	 */
	if (chbtyp != NCX_BT_LIST) {
	    /* setup next child if the cur child is 0 - 1 instance */
	    switch (obj_get_iqualval(curchild)) {
	    case NCX_IQUAL_ONE:
	    case NCX_IQUAL_OPT:
		if (chobj) {
		    chobj = obj_next_child(chobj);
		}
		break;
	    default:
		break;
	    }
	}
	xml_clean_node(&chnode);

	/* check if this is the special internal load-config RPC method,
	 * in which case there will not be an end tag for the <load-config>
	 * start tag passed to this function.  Need to quick exit
	 * to prevent reading past the end of the XML config file,
	 * which just contains the <config> element
	 *
	 * know there is just one parameter named <config> so just
	 * go through the loop once because the </load-config> tag
	 * will not be present
	 */
	if ((startnode->nsid == xmlns_ncx_id() || startnode->nsid == 0) && 
	    !xml_strcmp(startnode->elname, NCX_EL_LOAD_CONFIG)) {
	    done = TRUE;
	}
    }

    xml_clean_node(&chnode);
    return retres;

} /* parse_complex_nc */


/********************************************************************
* FUNCTION parse_metadata_nc
* 
* Parse all the XML attributes in the specified xml_node_t struct
*
* Only XML_NT_START or XML_NT_EMPTY nodes will have attributes
*
* INPUTS:
*     scb == session control block
*     msg == incoming RPC message
*            Errors are appended to msg->errQ
*     obj == object template containing meta-data definition
*     nserr == TRUE if namespace errors should be checked
*           == FALSE if not, and any attribute is accepted 
*     node == node of the parameter maybe with attributes to be parsed
*     retval ==  val_value_t that should get the results of the parsing
*     
* OUTPUTS:
*    *retval will be filled in
*    msg->errQ may be appended with new errors or warnings
*    retval->editop contains the last value of the operation attribute
*      seen, if any; will be OP_EDITOP_NONE if not set
*
* RETURNS:
*   status
*********************************************************************/
static status_t 
    parse_metadata_nc (ses_cb_t *scb,
		       xml_msg_hdr_t *msg,
		       const obj_template_t *obj,
		       const xml_node_t *node,
		       boolean nserr,
		       val_value_t  *retval)
{
    const obj_metadata_t    *meta;
    const typ_def_t         *metadef;
    xml_attr_t              *attr;
    val_value_t             *metaval;
    xmlns_id_t               ncid, yangid;
    status_t                 res, retres;
    boolean                  iskey, isvalue;

    retres = NO_ERR;
    ncid =  xmlns_nc_id();
    yangid =  xmlns_yang_id();

    /* go through all the attributes in the node and convert
     * to val_value_t structs
     * find the correct typedef to match each attribute
     */
    for (attr = xml_get_first_attr(node);
	 attr != NULL;
	 attr = xml_next_attr(attr)) {

	if (attr->attr_dname && 
	    !xml_strncmp(attr->attr_dname, 
			 XMLNS, xml_strlen(XMLNS))) {
	    /* skip this 'xmlns' attribute */
	    continue;   
	}

	/* init per-loop locals */
	res = NO_ERR;
	meta = NULL;
	metadef = NULL;
	iskey = FALSE;
	isvalue = FALSE;

	/* check qualified and unqualified operation attribute,
	 * then the 'xmlns' attribute, then a defined attribute
	 */
	if (val_match_metaval(attr, ncid, NC_OPERATION_ATTR_NAME)) {
	    retval->editop = op_editop_id(attr->attr_val);
	    if (retval->editop == OP_EDITOP_NONE) {
		res = ERR_NCX_INVALID_VALUE;
	    } else {
		continue;
	    }
	} else if (val_match_metaval(attr, yangid, YANG_K_INSERT)) {
	    retval->insertop = op_insertop_id(attr->attr_val);
	    if (retval->insertop == OP_INSOP_NONE) {
		res = ERR_NCX_INVALID_VALUE;
	    } else {
		continue;
	    }
	} else if (val_match_metaval(attr, yangid, YANG_K_KEY)) {
	    iskey = TRUE;
	} else if (val_match_metaval(attr, yangid, YANG_K_VALUE)) {
	    isvalue = TRUE;
	} else {
	    /* find the attribute definition in this typdef */
	    meta = obj_find_metadata(obj, attr->attr_name);
	    if (meta) {
		metadef = meta->typdef;
	    } else if (!nserr) {
		metadef = typ_get_basetype_typdef(NCX_BT_STRING);
	    }
	}

	if (iskey || isvalue) {
	    metadef = typ_get_basetype_typdef(NCX_BT_STRING);
	}

	if (metadef) {
	    /* alloc a new value struct for rhe attribute */
	    metaval = val_new_value();
	    if (!metaval) {
		res = ERR_INTERNAL_MEM;
	    } else {
		/* parse the attribute string against the typdef */
		res = val_parse_meta(metadef, attr->attr_ns,
				     attr->attr_name, 
				     attr->attr_val, metaval);
		if (res == NO_ERR) {
		    dlq_enque(metaval, &retval->metaQ);
		} else {
		    val_free_value(metaval);
		}
	    }
	} else {
	    res = ERR_NCX_UNKNOWN_ATTRIBUTE;
	}

	if (res != NO_ERR) {
	    agt_record_attr_error(scb, msg, 
				  NCX_LAYER_OPERATION, res,  
				  attr, node, NULL, 
				  NCX_NT_VAL, retval);
	    CHK_EXIT;
	}
    }
    return retres;

} /* parse_metadata_nc */


/********************************************************************
* FUNCTION metadata_inst_check
* 
* Validate that all the XML attributes in the specified 
* xml_node_t struct are pesent in appropriate numbers
*
* Since attributes are unordered, they all have to be parsed
* before they can be checked for instance count
*
* INPUTS:
*     scb == session control block
*     msg == incoming RPC message
*            Errors are appended to msg->errQ
*     val == value to check for metadata errors
*     
* OUTPUTS:
*    msg->errQ may be appended with new errors or warnings
*
* RETURNS:
*   status
*********************************************************************/
static status_t 
    metadata_inst_check (ses_cb_t *scb,
			 xml_msg_hdr_t *msg,
			 val_value_t  *val)
{
    const obj_metadata_t *meta;
    uint32                cnt;
    status_t              res, retres;
    xmlns_qname_t         qname;
    xmlns_id_t            yangid;

    retres = NO_ERR;
    yangid = xmlns_yang_id();

    /* first check the inst count of the YANG attributes
     * may not need to do this at all if XmlTextReader
     * rejects duplicate XML attributes
     */
    cnt = val_metadata_inst_count(val, yangid, YANG_K_KEY);
    if (cnt > 1) {
	retres = ERR_NCX_EXTRA_ATTR;
	agt_record_error(scb, msg, NCX_LAYER_CONTENT, retres, 
			 NULL, NCX_NT_STRING, YANG_K_KEY, 
			 NCX_NT_VAL, val);
    }

    cnt = val_metadata_inst_count(val, yangid, YANG_K_VALUE);
    if (cnt > 1) {
	retres = ERR_NCX_EXTRA_ATTR;
	agt_record_error(scb, msg, NCX_LAYER_CONTENT, retres, 
			 NULL, NCX_NT_STRING, YANG_K_VALUE, 
			 NCX_NT_VAL, val);
    }

    if (!val->obj) {
	return retres;
    }

    for (meta = obj_first_metadata(val->obj);
	 meta != NO_ERR;
	 meta = obj_next_metadata(meta)) {

	res = NO_ERR;
	cnt = val_metadata_inst_count(val, meta->nsid, meta->name);

	/* check the instance qualifier from the metadata
	 * continue the loop if there is no error
	 */
	if (cnt > 1) {
	    res = ERR_NCX_EXTRA_ATTR;
	    qname.nsid = meta->nsid;
	    qname.name = meta->name;
	    agt_record_error(scb, msg, NCX_LAYER_CONTENT, 
			     res, NULL,
			     NCX_NT_QNAME, &qname, 
			     NCX_NT_VAL, val);
	    retres = res;
	}
    }
    return retres;

} /* metadata_inst_check */


/********************************************************************
* FUNCTION parse_btype_nc
* 
* Switch to dispatch to specific base type handler
*
* INPUTS:
*     scb == session control block
*            Input is read from scb->reader.
*     msg == incoming RPC message
*            Errors are appended to msg->errQ
*     obj == object template to use for parsing
*     startnode == top node of the parameter to be parsed
*            Parser function will attempt to consume all the
*            nodes until the matching endnode is reached
*     parentdc == data class of the parent node, which will get
*                 used if it is not explicitly set for the typdef
*     retval ==  val_value_t that should get the results of the parsing
*     
* OUTPUTS:
*    *retval will be filled in
*    msg->errQ may be appended with new errors or warnings
*
* RETURNS:
*   status
*********************************************************************/
static status_t 
    parse_btype_nc (ses_cb_t  *scb,
		    xml_msg_hdr_t *msg,
		    const obj_template_t *obj,
		    const xml_node_t *startnode,
		    ncx_data_class_t parentdc,
		    val_value_t  *retval)
{
    ncx_btype_t  btyp;
    status_t     res, res2, res3;
    boolean      nserr;

    btyp = obj_get_basetype(obj);

    /* get the attribute values from the start node */
    retval->editop = OP_EDITOP_NONE;
    retval->nsid = startnode->nsid;

    /* check namespace errors except if the type is ANY */
    nserr = (btyp != NCX_BT_ANY);

    /* parse the attributes, if any; do not quick exit on this error */
    res2 = parse_metadata_nc(scb, msg, obj, startnode, 
			     nserr, retval);

    /* continue to parse the startnode depending on the base type 
     * to record as many errors as possible
     */
    switch (btyp) {
    case NCX_BT_ANY:
	res = parse_any_nc(scb, msg, startnode, parentdc, retval);
	break;
    case NCX_BT_ENUM:
	res = parse_enum_nc(scb, msg, obj, startnode, parentdc, retval);
	break;
    case NCX_BT_EMPTY:
	res = parse_empty_nc(scb, msg, obj, startnode, parentdc, retval);
	break;
    case NCX_BT_BOOLEAN:
	res = parse_boolean_nc(scb, msg, obj, startnode, parentdc, retval);
	break;
    case NCX_BT_INT8:
    case NCX_BT_INT16:
    case NCX_BT_INT32:
    case NCX_BT_INT64:
    case NCX_BT_UINT8:
    case NCX_BT_UINT16:
    case NCX_BT_UINT32:
    case NCX_BT_UINT64:
    case NCX_BT_FLOAT32:
    case NCX_BT_FLOAT64:
	res = parse_num_nc(scb, msg, obj, btyp, startnode, 
			   parentdc, retval);
	break;
    case NCX_BT_KEYREF:
    case NCX_BT_STRING:
    case NCX_BT_BINARY:
    case NCX_BT_SLIST:
    case NCX_BT_BITS:
    case NCX_BT_INSTANCE_ID:
	res = parse_string_nc(scb, msg, obj, btyp, startnode, 
			      parentdc, retval);
	break;
    case NCX_BT_UNION:
	res = parse_union_nc(scb, msg, obj, startnode, parentdc, retval);
	break;
    case NCX_BT_CONTAINER:
    case NCX_BT_LIST:
	res = parse_complex_nc(scb, msg, obj, startnode, 
			       parentdc, retval);
	break;
    default:
	return SET_ERROR(ERR_INTERNAL_VAL);
    }

    /* set the config flag for this value */
    res3 = NO_ERR;

    if (res == NO_ERR  && res2 == NO_ERR) {
	/* this has to be after the retval typdef is set */
	res3 = metadata_inst_check(scb, msg, retval);
    }

    if (res != NO_ERR) {
	retval->res = res;
	return res;
    } else if (res2 != NO_ERR) {
	retval->res = res2;
	return res2;
    }

    retval->res = res3;
    return res3;

} /* parse_btype_nc */


/**************    E X T E R N A L   F U N C T I O N S **********/


/********************************************************************
* FUNCTION agt_val_parse_nc
* 
* Parse NETCONF PDU sub-contents into value fields
* This module does not enforce complex type completeness.
* Different subsets of configuration data are permitted
* in several standard (and any proprietary) RPC methods
*
* Makes sure that only allowed value strings
* or child nodes (and their values) are entered.
*
* Defaults are not added to any objects
* Missing objects are not checked
*
* A seperate parsing phase is used to fully validate the input
* contained in the returned val_value_t struct.
*
* This parsing phase checks that simple types are complete
* and child members of complex types are valid (but maybe 
* missing or incomplete child nodes.
*
* Note that the NETCONF inlineFilterType will be parsed
* correctly because it is type 'anyxml'.  This type is
* parsed as a struct, and no validation other well-formed
* XML is done.
*
* INPUTS:
*     scb == session control block
*     msg == incoming RPC message
*     obj == obj_template_t for the object to parse
*     startnode == top node of the parameter to be parsed
*     parentdc == parent data class
*                 For the first call to this function, this will
*                 be the data class of a parameter.
*     retval == address of initialized return value
*     
* OUTPUTS:
*    *status will be filled in
*     msg->errQ may be appended with new errors
*
* RETURNS:
*    status
*********************************************************************/
status_t
    agt_val_parse_nc (ses_cb_t  *scb,
		      xml_msg_hdr_t *msg,
		      const obj_template_t *obj,
		      const xml_node_t *startnode,
		      ncx_data_class_t  parentdc,
		      val_value_t  *retval)
{
    status_t res;

#ifdef DEBUG
    if (!scb || !msg || !obj || !startnode || !retval) {
	/* non-recoverable error */
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

#ifdef AGT_VAL_PARSE_DEBUG
    log_debug3("\nagt_val_parse: %s:%s btyp:%s", 
	       obj_get_mod_prefix(obj),
	       obj_get_name(obj), 
	       tk_get_btype_sym(obj_get_basetype(obj)));
#endif

    /* get the element values */
    res = parse_btype_nc(scb, msg, obj, startnode, 
			 parentdc, retval);
    return res;

}  /* agt_val_parse_nc */


#ifdef DEBUG
/********************************************************************
* FUNCTION agt_val_parse_test
* 
* scaffold code to get agt_val_parse tested 
*
* INPUTS:
*   testfile == NETCONF PDU in a test file
*
*********************************************************************/
void
    agt_val_parse_test (const char *testfile)
{
    ses_cb_t  *scb;
    status_t   res;

    /* create a dummy session control block */
    scb = agt_ses_new_dummy_session();
    if (!scb) {
	SET_ERROR(ERR_INTERNAL_MEM);
	return;
    }

    /* open the XML reader */
    res = xml_get_reader_from_filespec(testfile, &scb->reader);
    if (res != NO_ERR) {
	SET_ERROR(res);
	agt_ses_free_dummy_session(scb);
	return;
    }

    /* dispatch the test PDU */
    agt_top_dispatch_msg(scb);

    /* clean up and exit */
    agt_ses_free_dummy_session(scb);

}  /* agt_val_parse_test */
#endif



/* END file agt_val_parse.c */

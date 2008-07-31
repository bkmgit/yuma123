/*  FILE: xsd_typ.c

    Generate the XSD type constructs for yangdump:

    - simpleType
    - complexType
    	
*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
24nov06      abb      split out from xsd.c

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <ctype.h>

#include <xmlstring.h>
#include <xmlreader.h>

#ifndef _H_procdefs
#include  "procdefs.h"
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

#ifndef _H_ncx
#include "ncx.h"
#endif

#ifndef _H_ncxconst
#include "ncxconst.h"
#endif

#ifndef _H_ncxtypes
#include "ncxtypes.h"
#endif

#ifndef _H_ncxmod
#include "ncxmod.h"
#endif

#ifndef _H_obj
#include "obj.h"
#endif

#ifndef _H_rpc
#include "rpc.h"
#endif

#ifndef _H_status
#include  "status.h"
#endif

#ifndef _H_typ
#include "typ.h"
#endif

#ifndef _H_val
#include "val.h"
#endif

#ifndef _H_xmlns
#include "xmlns.h"
#endif

#ifndef _H_xml_util
#include "xml_util.h"
#endif

#ifndef _H_xml_val
#include "xml_val.h"
#endif

#ifndef _H_xsd_typ
#include "xsd_typ.h"
#endif

#ifndef _H_xsd_util
#include "xsd_util.h"
#endif

#ifndef _H_yang
#include "yang.h"
#endif


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/

/* memberTypes per line hack */
#define PER_LINE 2

/* forward declarations for recursive child node processor */
static status_t
    do_elem_btype (const ncx_module_t *mod,
		   const typ_child_t *typch,
		   val_value_t *val);


/********************************************************************
* FUNCTION add_err_annotation
* 
*   Check if an annotation element is needed
*   If so, create error-app-tag and/or error-message
*   appinfo elements and add the result to 'val
*
* INPUTS:
*    errmsg  == error-message value
*    errtag == error-app-tag value
*    descr == description clause
*    ref == reference clause
*    val == value struct to add annotation as a child node
* 
* RETURNS:
*   status
*********************************************************************/
static status_t
    add_err_annotation (const xmlChar *errmsg,
			const xmlChar *errtag,
			const xmlChar *descr,
			const xmlChar *ref,
			val_value_t  *val)
{
    val_value_t   *anval, *chval;
    status_t       res;

    /* create the pattern element */
    if (errmsg || errtag || descr || ref) {
	anval = xml_val_new_struct(XSD_ANNOTATION, xmlns_xs_id());
	if (!anval) {
	    return ERR_INTERNAL_MEM;
	} else {
	    val_add_child(anval, val);
	}

	if (descr) {
	    res = xsd_do_documentation(descr, anval);
	    if (res != NO_ERR) {
		return res;
	    }
	}

	if (errmsg || errtag || ref) {
	    chval = xsd_make_err_appinfo(ref, errmsg, errtag);
	    if (!chval) {
		return ERR_INTERNAL_MEM;
	    } else {
		val_add_child(chval, anval);
	    }
	}
    }
    return NO_ERR;

}   /* add_err_annotation */


/********************************************************************
* FUNCTION add_pattern
* 
*   Set up a new pattern and add it as a child node of 'val'
*
* INPUTS:
*    typdef == typdef in progress
*    patstr == pattern string to add
*    val == value struct to hold the pattern element
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    add_pattern (const typ_def_t *typdef,
		 const xmlChar *patstr,
		 val_value_t *val)
{
    val_value_t           *patval;
    const ncx_errinfo_t   *errinfo;
    const xmlChar         *errmsg, *errtag, *descr, *ref;
    const typ_def_t       *tdef;
    status_t               res;
    xmlns_id_t             xsd_id;

    errinfo = NULL;
    errmsg = NULL;
    errtag = NULL;
    descr = NULL;
    ref = NULL;
    res = NO_ERR;

    /* check if appinfo provided for the range; if so,
     * add it to the restriction of all the range-part simpleTypes
     */
    if (typdef->class == NCX_CL_NAMED) {
	tdef = typ_cget_new_named(typdef);
	errinfo = tdef->pat_errinfo;
    } else {
	errinfo = typdef->pat_errinfo;
    }

    if (errinfo) {
	errmsg = errinfo->error_message;
	errtag = errinfo->error_app_tag;
	descr = errinfo->descr;
	ref = errinfo->ref;
    }
	
    xsd_id = xmlns_xs_id();

    /* create the pattern element */
    if (errmsg || errtag || descr || ref) {
	patval = xml_val_new_struct(NCX_EL_PATTERN, xsd_id);
	if (!patval) {
	    return ERR_INTERNAL_MEM;
	} else {
	    val_add_child(patval, val);
	    res = add_err_annotation(errmsg, errtag, 
				     descr, ref, patval);
	    if (res != NO_ERR) {
		return res;
	    }
		
	}
    } else {
	patval = xml_val_new_flag(NCX_EL_PATTERN, xsd_id);
	if (!patval) {
	    return ERR_INTERNAL_MEM;
	} else {
	    val_add_child(patval, val);
	}
    }

    res = xml_val_add_cattr(NCX_EL_VALUE, 0, patstr, patval);

    return res;

}   /* add_pattern */


/********************************************************************
* FUNCTION new_restriction
* 
*   Set up a new restriction struct
*   This is not complete if the 'hasnodes' parameter is TRUE
*
* INPUTS:
*    mod == module in progress (need target namespace ID)
*    base_nsid == namespace ID of the basetype name
*    basename == base type that this is a restriction from
*    hasnodes == TRUE if there will be child elements added
*             == FALSE if this restriction has no child nodes
* 
* RETURNS:
*   new struct or NULL if malloc error
*********************************************************************/
static val_value_t *
    new_restriction (const ncx_module_t  *mod,
		     xmlns_id_t     base_nsid,
		     const xmlChar *basename,
		     boolean        hasnodes)

{
    val_value_t *val;
    xmlChar     *str;
    status_t     res;
    xmlns_id_t   xsd_id;

    xsd_id = xmlns_xs_id();

    /* create a struct named 'restriction' to hold the entire result */
    if (hasnodes) {
	val = xml_val_new_struct(XSD_RESTRICTION, xsd_id);
    } else {
	val = xml_val_new_flag(XSD_RESTRICTION, xsd_id);
    }
    if (!val) {
	return NULL;
    }

    /* add the base=QName attribute */
    if (mod->nsid == base_nsid) {
	/* do not use the prefix if this is the target namespace */
	res = xml_val_add_cattr(XSD_BASE, 0, basename, val);
    } else {
	/* not the target namespace, so need a QName instead of Name */
	str = xml_val_make_qname(base_nsid, basename);
	if (str) {
	    res = xml_val_add_attr(XSD_BASE, 0, str, val);
	    if (res != NO_ERR) {
		m__free(str);
	    }
	} else {
	    res = ERR_INTERNAL_MEM;
	}
    }

    if (res != NO_ERR) {
	val_free_value(val);
	return NULL;
    } else {
	return val;
    }
    /*NOTREACHED*/

}   /* new_restriction */


/********************************************************************
* FUNCTION add_enum_restriction
* 
*   Add an enumeration element to the parent restriction element
*
* INPUTS:
*    enu == enum struct (contains name, description, etc.)
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->v.childQ has entries added for the types in this module
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    add_enum_restriction (const typ_enum_t *enu,
			  val_value_t *val)
{
    val_value_t      *enuval, *annot, *appval;
    status_t          res;
    xmlns_id_t        xsd_id;

    xsd_id = xmlns_xs_id();

    enuval = xml_val_new_struct(XSD_ENUMERATION, xsd_id);
    if (!enuval) {
	return ERR_INTERNAL_MEM;
    } else {
	val_add_child(enuval, val);  /* add early */
    }

    /* add value attribute */
    res = xml_val_add_cattr(NCX_EL_VALUE, 0, enu->name, enuval);
    if (res != NO_ERR) {
	return res;
    }

    /* add annotation child element */
    annot = xml_val_new_struct(XSD_ANNOTATION, xsd_id);
    if (!annot) {
	return ERR_INTERNAL_MEM;
    } else {
	val_add_child(annot, enuval);  /* add early */
    }

    /* add documentation child element */
    if (enu->descr) {
	res = xsd_do_documentation(enu->descr, annot);
	if (res != NO_ERR) {
	    return res;
	}
    }

    if (enu->ref) {
	res = xsd_do_reference(enu->ref, annot);
	if (res != NO_ERR) {
	    return res;
	}
    }

    /* add appinfo child element */
    if (enu->flags & TYP_FL_ISBITS) {
	appval = xsd_make_bit_appinfo(enu->pos, 
				      &enu->appinfoQ,
				      enu->status);
    } else {
	appval = xsd_make_enum_appinfo(enu->val, 
				       &enu->appinfoQ,
				       enu->status);
    }
    if (!appval) {
	return ERR_INTERNAL_MEM;
    } else {
	val_add_child(appval, annot);
    }

    return NO_ERR;

}   /* add_enum_restriction */


/********************************************************************
* FUNCTION add_sval_restriction
* 
*   Add an enumeration element to the parent restriction element
*
* INPUTS:
*    enu == enum struct (contains name, description, etc.)
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->v.childQ has entries added for the types in this module
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    add_sval_restriction (const xmlChar *strval,
			  val_value_t *val)
{
    val_value_t      *enuval;
    status_t          res;
    xmlns_id_t        xsd_id;

    xsd_id = xmlns_xs_id();

    enuval = xml_val_new_struct(XSD_ENUMERATION, xsd_id);
    if (!enuval) {
	return ERR_INTERNAL_MEM;
    } else {
	val_add_child(enuval, val);  /* add early */
    }

    /* add value attribute */
    res = xml_val_add_cattr(NCX_EL_VALUE, 0, strval, enuval);
    return res;

}   /* add_sval_restriction */


/********************************************************************
* FUNCTION make_enum
* 
*   Create a value struct representing an enumerated type
*   This is complete; The val_free_value function must be called
*   with the return value if it is non-NULL;
*
* INPUTS:
*   mod = module in progress
*   typdef == typ def for the type definition template
*
* RETURNS:
*   value struct or NULL if malloc error
*********************************************************************/
static val_value_t *
    make_enum (const ncx_module_t *mod,
	       const typ_def_t *typdef)
{
    val_value_t      *chval;
    const typ_enum_t *typ_enu;
    status_t          res;

    /* build a restriction element */
    chval = new_restriction(mod, xmlns_xs_id(), NCX_EL_STRING, TRUE);
    if (!chval) {
	return NULL;
    }

    /* add all the enumeration strings */
    for (typ_enu = typ_first_con_enumdef(typdef);
	 typ_enu != NULL;
	 typ_enu = (const typ_enum_t *)dlq_nextEntry(typ_enu)) {
	res = add_enum_restriction(typ_enu, chval);
	if (res != NO_ERR) {
	    val_free_value(chval);
	    return NULL;
	}
    }

    return chval;

}   /* make_enum */


/********************************************************************
* FUNCTION finish_range
* 
*   Generate XSD constructs for a range
*
* INPUTS:
*    typdef == typ def for the type definition in progress
*    rdef == first rangedef record (1 of N)
*    val == struct parent to contain child nodes for each range
*
* OUTPUTS:
*    val->v.childQ has entries added for the ranges found
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    finish_range (const typ_def_t *typdef,
		  const typ_rangedef_t *rdef,
		  val_value_t *val)
{
    val_value_t          *chval;
    const ncx_errinfo_t  *errinfo;
    const xmlChar        *errmsg, *errtag, *descr, *ref;
    const typ_def_t      *tdef;
    xmlChar               numbuff[NCX_MAX_NUMLEN];
    uint32                len;
    xmlns_id_t            xsd_id;
    boolean               skipit, srange;
    status_t              res;

    xsd_id = xmlns_xs_id();
    errinfo = NULL;
    errmsg = NULL;
    errtag = NULL;
    descr = NULL;
    ref = NULL;

    /* check if appinfo provided for the range; if so,
     * add it to the restriction of all the range-part simpleTypes
     */
    if (typdef->class == NCX_CL_NAMED) {
	tdef = typ_cget_new_named(typdef);
	errinfo = tdef->range_errinfo;
    } else {
	errinfo = typdef->range_errinfo;
    }

    if (errinfo) {
	errmsg = errinfo->error_message;
	errtag = errinfo->error_app_tag;
	descr = errinfo->descr;
	ref = errinfo->ref;
    }

    skipit = FALSE;
    switch (typ_get_basetype(typdef)) {
    case NCX_BT_STRING:
    case NCX_BT_BINARY:
	srange = TRUE;
	break;
    default:
	srange = FALSE;
    }

    /* handle minInclusive first */
    switch (rdef->btyp) {
    case NCX_BT_INT8:
    case NCX_BT_INT16:
    case NCX_BT_INT32:
    case NCX_BT_INT64:
    case NCX_BT_UINT8:
    case NCX_BT_UINT16:
    case NCX_BT_UINT32:
    case NCX_BT_UINT64:
	if (rdef->flags & TYP_FL_LBMIN) {
	    skipit = TRUE;
	} else {
	    res = ncx_sprintf_num(numbuff, &rdef->lb, rdef->btyp, &len);
	    if (res != NO_ERR) {
		return res;
	    }
	}
	break;
    case NCX_BT_FLOAT32:
    case NCX_BT_FLOAT64:
	if (rdef->flags & TYP_FL_LBINF) {
	    skipit = TRUE;
	} else {
	    res = ncx_sprintf_num(numbuff, &rdef->lb, rdef->btyp, &len);
	    if (res != NO_ERR) {
		return res;
	    }
	}
	break;
    default:
	return SET_ERROR(ERR_INTERNAL_VAL);
    }

    if (!skipit) {
	if (errmsg || errtag || descr || ref) {
	    if (srange) {
		chval = xml_val_new_struct(XSD_MIN_LEN, xsd_id);
	    } else {
		chval = xml_val_new_struct(XSD_MIN_INCL, xsd_id);
	    }
	} else {
	    if (srange) {
		chval = xml_val_new_flag(XSD_MIN_LEN, xsd_id);
	    } else {
		chval = xml_val_new_flag(XSD_MIN_INCL, xsd_id);
	    }
	}
	if (!chval) {
	    return ERR_INTERNAL_MEM;
	} else {
	    val_add_child(chval, val);
	}

	res = xml_val_add_cattr(NCX_EL_VALUE, 0, numbuff, chval);
	if (res != NO_ERR) {
	    return res;
	}

	if (errmsg || errtag || descr || ref) {
	    res = add_err_annotation(errmsg, errtag,
				     descr, ref, chval);
	    if (res != NO_ERR) {
		return res;
	    }
	}
    }

    /* handle maxInclusive next */
    skipit = FALSE;
    switch (rdef->btyp) {
    case NCX_BT_INT8:
    case NCX_BT_INT16:
    case NCX_BT_INT32:
    case NCX_BT_INT64:
    case NCX_BT_UINT8:
    case NCX_BT_UINT16:
    case NCX_BT_UINT32:
    case NCX_BT_UINT64:
	if (rdef->flags & TYP_FL_UBMAX) {
	    skipit = TRUE;
	} else {
	    res = ncx_sprintf_num(numbuff, &rdef->ub, rdef->btyp, &len);
	    if (res != NO_ERR) {
		return res;
	    }
	}
	break;
    case NCX_BT_FLOAT32:
    case NCX_BT_FLOAT64:
	if (rdef->flags & TYP_FL_UBINF) {
	    skipit = TRUE;
	} else {
	    res = ncx_sprintf_num(numbuff, &rdef->ub, rdef->btyp, &len);
	    if (res != NO_ERR) {
		return res;
	    }
	}
	break;
    default:
	return SET_ERROR(ERR_INTERNAL_VAL);
    }

    if (!skipit) {
	if (errmsg || errtag || descr || ref) {
	    if (srange) {
		chval = xml_val_new_struct(XSD_MAX_LEN, xsd_id);
	    } else {
		chval = xml_val_new_struct(XSD_MAX_INCL, xsd_id);
	    }
	} else {
	    if (srange) {
		chval = xml_val_new_flag(XSD_MAX_LEN, xsd_id);
	    } else {
		chval = xml_val_new_flag(XSD_MAX_INCL, xsd_id);
	    }
	}
	if (!chval) {
	    return ERR_INTERNAL_MEM;
	} else {
	    val_add_child(chval, val);
	}

	res = xml_val_add_cattr(NCX_EL_VALUE, 0, numbuff, chval);
	if (res != NO_ERR) {
	    return res;
	}

	if (errmsg || errtag || descr || ref) {
	    res = add_err_annotation(errmsg, errtag,
				     descr, ref, chval);
	    if (res != NO_ERR) {
		return res;
	    }
	}
    }

    return NO_ERR;

}   /* finish_range */


/********************************************************************
* FUNCTION new_range_union
* 
*   Create a union of simple types in order to handle
*   a range definition with multiple parts, such as:
*
*   range "min .. 4 | 7 | 11";
*
* INPUTS:
*    mod == module in progress (need target namespace ID)
*    rangeQ == Q of typ_rangedef_t
*    typdef == type definition for the simple or named type
*              generating the range
*    btyp == builtin type for typdef
*    base_id == NS id of the base type name
*    basename == name string for the base type name
*
* RETURNS:
*   new struct or NULL if malloc error
*********************************************************************/
static val_value_t *
    new_range_union (const ncx_module_t  *mod,
		     const dlq_hdr_t *rangeQ,
		     const typ_def_t *typdef,
		     ncx_btype_t btyp,
		     xmlns_id_t base_id,
		     const xmlChar *basename)
{
    const typ_def_t      *tdef;
    val_value_t          *val, *chval, *rval;
    const xmlChar        *patstr;
    const typ_sval_t     *typ_sval;
    const typ_rangedef_t *rv;
    status_t              res;
    xmlns_id_t            xsd_id;

    xsd_id = xmlns_xs_id();
    patstr = NULL;

    val = xml_val_new_struct(XSD_UNION, xsd_id);
    if (!val) {
	return NULL;
    }

    if (btyp==NCX_BT_STRING) {
	typ_sval = NULL;
	if (typdef->class==NCX_CL_SIMPLE) {
	    if (typdef->def.simple.strrest==NCX_SR_PATTERN) {
		typ_sval = typ_first_strdef(typdef);
	    }
	} else if (typdef->class==NCX_CL_NAMED) {
	    tdef = typ_cget_new_named(typdef);
	    if (tdef && tdef->def.simple.strrest==NCX_SR_PATTERN) {
		typ_sval = typ_first_strdef(tdef);
	    }
	}
	if (typ_sval) {
	    patstr = typ_sval->val;
	}
    }
	    
    /* go through the Q of range parts and add a simpleType
     * for each one
     */
    for (rv = (const typ_rangedef_t *)dlq_firstEntry(rangeQ);
	 rv != NULL;
	 rv = (const typ_rangedef_t *)dlq_nextEntry(rv)) {

	chval = xml_val_new_struct(XSD_SIM_TYP, xsd_id);
	if (!chval) {
	    val_free_value(val);
	    return NULL;
	} else {
	    val_add_child(chval, val);  /* add early */
	}

	rval = new_restriction(mod, base_id, basename, TRUE);
	if (!rval) {
	    val_free_value(val);
	    return NULL;
	} else {
	    val_add_child(rval, chval);  /* add early */
	}

	res = finish_range(typdef, rv, rval);
	if (res != NO_ERR) {
	    val_free_value(val);
	    return NULL;
	}

	if (patstr) {
	    res = add_pattern(typdef, patstr, rval);
	    if (res != NO_ERR) {
		val_free_value(val);
		return NULL;
	    }
	}
    }

    return val;

}   /* new_range_union */


/********************************************************************
* FUNCTION finish_list
* 
*   Generate the XSD for a list element 
*
* INPUTS:
*    mod == module in progress
*    btyp == type of the typedef (NCX_BT_SLIST or NCX_BT_XLIST)
*    typdef == typdef for the list
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->v.childQ has entries added for the nodes in this typdef
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    finish_list (const ncx_module_t *mod,
		 ncx_btype_t  btyp,
		 const typ_def_t *typdef,
		 val_value_t *val)
{
    const typ_template_t *listtyp;
    const typ_def_t      *rtypdef;
    const ncx_num_t      *lb, *ub;
    const xmlChar        *typname;
    val_value_t          *top, *simtyp, *rest, *lval;
    status_t              res;
    int32                 ret;
    uint32                len;
    xmlns_id_t            xsd_id, list_id;
    ncx_btype_t           range_btyp;
    xmlChar              *str, numbuff[NCX_MAX_NLEN];

    res = NO_ERR;
    xsd_id = xmlns_xs_id();

    /* get the list member typedef */
    if (btyp == NCX_BT_SLIST) {
	listtyp = typ_get_listtyp(typdef);
	list_id = typ_get_nsid(listtyp);
	typname = listtyp->name;
    } else {
	list_id = xsd_id;
	typname = NCX_EL_STRING;
    }

    /* see if there are any length restrictions on the list typedef
     * not the list member -- this controls the number of entries 
     * allowed
     */
    rtypdef = typ_get_cqual_typdef(typdef, NCX_SQUAL_RANGE);
    if (rtypdef) {
	/* complex form with inline restriction */
	top = xml_val_new_struct(XSD_LIST, xsd_id);
	if (!top) {
	    return ERR_INTERNAL_MEM;
	}
    } else {
	/* simple form with itemType='QName' */
	top = xml_val_new_flag(XSD_LIST, xsd_id);
	if (top) {
	    /* construct the itemType="foo:bar" attribute */
	    if (list_id != mod->nsid) {
		str = xml_val_make_qname(list_id, typname);
	    } else {
		str = xml_strdup(typname);
	    }
	    if (!str) {
		val_free_value(top);
		return ERR_INTERNAL_MEM;
	    }
	    /* add the itemType attribute to the list element */
	    res = xml_val_add_attr(XSD_ITEMTYPE, 0, str, top);
	    if (res != NO_ERR) {
		m__free(str);
		val_free_value(top);
		return res;
	    }
	} else {
	    return ERR_INTERNAL_MEM;
	}
    }

    /* add the length constraints if needed */
    if (rtypdef) {
	res = typ_get_rangebounds_con(rtypdef, &range_btyp,&lb, &ub);
	if (res != NO_ERR) {
	    /* should not happen since rtypdef is non-NULL */
	    return SET_ERROR(res);
	}

	/* need a simple type wrapper */
	simtyp = xml_val_new_struct(XSD_SIM_TYP, xsd_id);
	if (!simtyp) {
	    val_free_value(top);
	    return ERR_INTERNAL_MEM;
	} else {
	    val_add_child(simtyp, top);
	}

	/* need a restriction wrapper */
	rest = new_restriction(mod, list_id, typname, TRUE);
	if (!rest) {
	    val_free_value(top);
	    return ERR_INTERNAL_MEM;
	} else {
	    val_add_child(rest, simtyp);
	}

	/* check fixed length or a range given */
	ret = ncx_compare_nums(lb, ub, range_btyp);
	if (ret) {
	    /* range given, gen lower bound if not zero */
	    if (!ncx_num_zero(lb, range_btyp)) {
		lval = xml_val_new_flag(XSD_MIN_LEN, xsd_id);
		if (!lval) {
		    res = ERR_INTERNAL_MEM;
		} else {
		    val_add_child(lval, rest);
		    res = ncx_sprintf_num(numbuff, lb, range_btyp, &len);
		    if (res == NO_ERR) {
			res = xml_val_add_cattr(XSD_VALUE, 0, numbuff, lval);
		    }
		}
		if (res != NO_ERR) {
		    val_free_value(top);
		    return res;
		}
	    }

	    /* gen the upper bound */
	    lval = xml_val_new_flag(XSD_MAX_LEN, xsd_id);
	    if (!lval) {
		res = ERR_INTERNAL_MEM;
	    } else {
		val_add_child(lval, rest);
		res = ncx_sprintf_num(numbuff, ub, range_btyp, &len);
		if (res == NO_ERR) {
		    res = xml_val_add_cattr(XSD_VALUE, 0, numbuff, lval);
		}
	    }
	} else {
	    /* lb and ub equal, fixed range given */
	    lval = xml_val_new_flag(XSD_LENGTH, xsd_id);
	    if (!lval) {
		res = ERR_INTERNAL_MEM;
	    } else {
		val_add_child(lval, rest);
		res = xml_val_add_cattr(XSD_FIXED, 0, XSD_TRUE, lval);
		if (res == NO_ERR) {
		    res = ncx_sprintf_num(numbuff, lb, range_btyp, &len);
		    if (res == NO_ERR) {
			res = xml_val_add_cattr(XSD_VALUE, 0, numbuff, lval);
		    }
		}
	    }
	}
	if (res != NO_ERR) {
	    val_free_value(top);
	    return res;
	}
    }

    val_add_child(top, val);
    return NO_ERR;

}   /* finish_list */


/********************************************************************
* FUNCTION do_elem_list
* 
*   Generate the element node for a list child type
*
* INPUTS:
*    mod == module conversion in progress
*    typch == typ_child_t for the child node to use
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->v.childQ has entries added for the child type
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    do_elem_list (const ncx_module_t *mod,
		  const typ_child_t *typch,
		  val_value_t *val)
{
    val_value_t    *elem, *typnode;
    status_t        res;
    ncx_btype_t     btyp;

    /* check if this is a named type with a struct base type */
    if (typch->typdef.class == NCX_CL_NAMED) {
	elem = xsd_new_element(mod, typch->name, &typch->typdef, 
			       typch->parent, FALSE, FALSE);
	if (elem) {
	    val_add_child(elem, val);
	    return NO_ERR;
	} else {
	    return ERR_INTERNAL_MEM;
	}
    }

    /* element list of 1 simple content node */
    elem = xsd_new_element(mod, typch->name, &typch->typdef, 
			   typch->parent, TRUE, FALSE);
    if (!elem) {
	return ERR_INTERNAL_MEM;
    }

    /* next level is simpleType */
    typnode = xml_val_new_struct(XSD_SIM_TYP, xmlns_xs_id());
    if (!typnode) {
	val_free_value(elem);
	return ERR_INTERNAL_MEM;
    } else {
	val_add_child(typnode, elem);  /* add early */
    }

    /* generate the list element */
    btyp = typ_get_basetype(&typch->typdef);
    res = finish_list(mod, btyp, &typch->typdef, typnode);
    if (res != NO_ERR) {
	val_free_value(elem);
    } else {
	val_add_child(elem, val);
    }

    return res;

}   /* do_elem_list */


/********************************************************************
* FUNCTION do_elem_union
* 
*   Generate the element node for a union child type
*
* INPUTS:
*    mod == module conversion in progress
*    typch == typ_child_t for the child node to use
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->v.childQ has entries added for the child type
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    do_elem_union (const ncx_module_t *mod,
		   const typ_child_t *typch,
		   val_value_t *val)
{
    val_value_t    *elem, *typnode;
    status_t        res;

    /* check if this is a named type with a struct base type */
    if (typch->typdef.class == NCX_CL_NAMED) {
	elem = xsd_new_element(mod, typch->name, &typch->typdef, 
			       typch->parent, FALSE, FALSE);
	if (elem) {
	    val_add_child(elem, val);
	    return NO_ERR;
	} else {
	    return ERR_INTERNAL_MEM;
	}
    }

    /* element union of N simple types */
    elem = xsd_new_element(mod, typch->name, &typch->typdef, 
			   typch->parent, TRUE, FALSE);
    if (!elem) {
	return ERR_INTERNAL_MEM;
    }

    /* next level is simpleType */
    typnode = xml_val_new_struct(XSD_SIM_TYP, xmlns_xs_id());
    if (!typnode) {
	val_free_value(elem);
	return ERR_INTERNAL_MEM;
    } else {
	val_add_child(typnode, elem);  /* add early */
    }

    /* generate the union element */
    res = xsd_finish_union(mod, &typch->typdef, typnode);
    if (res != NO_ERR) {
	val_free_value(elem);
    } else {
	val_add_child(elem, val);
    }

    return res;

}   /* do_elem_union */


/********************************************************************
* FUNCTION finish_attributes
* 
*   Generate all the attribute nodes declared in this typ def
*
* INPUTS:
*    mod == module conversion in progress
*    typdef == typ_def for the typ_template struct to use for 
*              the attribute list source
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->v.childQ has entries added for the attributes in this typdef
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    finish_attributes (const ncx_module_t *mod,
		       const typ_def_t *typdef,
		       val_value_t *val)
{
    val_value_t         *aval;
    const typ_child_t   *typ_attr;
    const xmlChar       *name;
    ncx_iqual_t          iqual;
    status_t             res;
    xmlns_id_t           xsd_id;

    xsd_id = xmlns_xs_id();

    for (typ_attr = typ_first_meta_con(typdef);
	 typ_attr != NULL;
	 typ_attr = (typ_child_t *)dlq_nextEntry(typ_attr)) {

	/* get an attribute node */
	aval = xml_val_new_flag(XSD_ATTRIBUTE, xsd_id);
	if (!aval) {
	    return ERR_INTERNAL_MEM;
	}

	/* set the attribute name */
	res = xml_val_add_attr(NCX_EL_NAME, 0, typ_attr->name, aval);
	if (res != NO_ERR) {
	    val_free_value(aval);
	    return res;
	}

	/* set the data type */
	res = xsd_add_type_attr(mod, &typ_attr->typdef, aval);
	if (res != NO_ERR) {
	    val_free_value(aval);
	    return res;
	}

	/* add the use attribute */
	iqual = typ_get_iqualval_def(&typ_attr->typdef);
	switch (iqual) {
	case NCX_IQUAL_ONE:
	case NCX_IQUAL_1MORE:
	    res = xml_val_add_cattr(XSD_USE, 0, XSD_REQUIRED, aval);
	    break;
	case NCX_IQUAL_OPT:
	case NCX_IQUAL_ZMORE:
	    /* do nothing since this is the default */
	    /* res = xml_val_add_cattr(XSD_USE, 0, XSD_OPTIONAL, aval); */
	    break;
	case NCX_IQUAL_NONE:
	default:
	    res = SET_ERROR(ERR_INTERNAL_VAL);
	}
	if (res != NO_ERR) {
	    val_free_value(aval);
	    return res;
	}

	/* check if there is a default value */
	name = typ_get_default(&typ_attr->typdef);
	if (name) {
	    res = xml_val_add_cattr(NCX_EL_DEFAULT, 0, name, aval);
	    if (res != NO_ERR) {
		val_free_value(aval);
		return res;
	    }
	}

	/* add the attribute node to the parent node 
	 * inside a complexType 
	 */
	val_add_child(aval, val); 
    }

    return NO_ERR;

}   /* finish_attributes */



/********************************************************************
* FUNCTION finish_struct
* 
*   Generate the XSD for a struct data type
*
* INPUTS:
*    mod == module conversion in progress
*    typdef == typ_def for the typ_template struct to use
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->v.childQ has entries added for the attributes in this typdef
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    finish_struct (const ncx_module_t *mod,
		   const typ_def_t *typdef,
		   val_value_t *val)
{
    val_value_t            *top;
    const typ_child_t      *child;
    status_t                res;
    xmlns_id_t              xsd_id;

    res = NO_ERR;
    xsd_id = xmlns_xs_id();

    /* struct of N arbitrary nodes */
    top = xml_val_new_struct(XSD_SEQUENCE, xsd_id);
    if (!top) {
	return ERR_INTERNAL_MEM;
    }

    /* go through all the child nodes and generate elements for them */
    for (child = typ_first_child(&typdef->def.complex);
	 child != NULL && res==NO_ERR;
	 child = typ_next_con_child(child)) {
	res = do_elem_btype(mod, child, top);
    }

    if (res != NO_ERR) {
	val_free_value(top);
    } else {
	val_add_child(top, val);
    }

    /* generate the attributes allowed in the struct node */
    if (res == NO_ERR) {
	res = finish_attributes(mod, typdef, val);
    }

    return res;

}   /* finish_struct */


/********************************************************************
* FUNCTION do_elem_struct
* 
*   Generate the element node for a struct child type
*
* INPUTS:
*    mod == module conversion in progress
*    typch == typ_child_t for the child node to use
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->v.childQ has entries added for the child type
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    do_elem_struct (const ncx_module_t *mod,
		    const typ_child_t *typch,
		    val_value_t *val)
{
    val_value_t    *elem, *cpx;
    status_t        res;
    xmlns_id_t      xsd_id;

    /* check if this is a named type with a struct base type */
    if (typch->typdef.class == NCX_CL_NAMED) {
	elem = xsd_new_element(mod, typch->name, &typch->typdef, 
			       typch->parent, FALSE, FALSE);
	if (elem) {
	    val_add_child(elem, val);
	    return NO_ERR;
	} else {
	    return ERR_INTERNAL_MEM;
	}
    }

    xsd_id = xmlns_xs_id();

    /* element struct of N arbitrary nodes */
    elem = xsd_new_element(mod, typch->name, &typch->typdef, 
			   typch->parent, TRUE, FALSE);
    if (!elem) {
	return ERR_INTERNAL_MEM;
    }

    /* next level is complexType */
    cpx = xml_val_new_struct(XSD_CPX_TYP, xsd_id);
    if (!cpx) {
	val_free_value(elem);
	return ERR_INTERNAL_MEM;
    } else {
	val_add_child(cpx, elem);  /* add early */
    }

    /* finish the complexType */
    res = finish_struct(mod, &typch->typdef, cpx);
    if (res != NO_ERR) {
	val_free_value(elem);
    } else {
	val_add_child(elem, val);
    }

    return res;

}   /* do_elem_struct */


/********************************************************************
* FUNCTION finish_choice
* 
*   Generate the XSD for a choice data type
*
* INPUTS:
*    mod == module conversion in progress
*    typdef == typ_def for the typ_template struct to use
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->v.childQ has entries added for this typdef
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    finish_choice (const ncx_module_t *mod,
		   const typ_def_t *typdef,
		   val_value_t *val)
{
    val_value_t            *top, *grp;
    const typ_child_t      *child, *grchild;
    status_t                res;
    xmlns_id_t              xsd_id;

    xsd_id = xmlns_xs_id();
    res = NO_ERR;

    /* choice of N arbitrary nodes or groups */
    top = xml_val_new_struct(NCX_EL_CHOICE, xsd_id);
    if (!top) {
	return ERR_INTERNAL_MEM;
    }

    /* go through all the child nodes and generate elements 
     * for them.  The raw queue access functions are used
     * which should be changed to typ.h functions
     */
    for (child = (const typ_child_t *)
	     dlq_firstEntry(&typdef->def.complex.childQ);
	 child != NULL && res == NO_ERR;
	 child = (const typ_child_t *)dlq_nextEntry(child)) {

	/* check if this is a group or a simple node */
	if (!dlq_empty(&child->groupQ)) {
	    /* group of simple nodes */
	    grp = xml_val_new_struct(XSD_GROUP, xsd_id);
	    if (!grp) {
		res = ERR_INTERNAL_MEM;
	    } else {
		val_add_child(grp, top);  /* add early */
	    }
	    for (grchild = (typ_child_t *)dlq_firstEntry(&child->groupQ);
		 grchild != NULL && res == NO_ERR;
		 grchild = (typ_child_t *)dlq_nextEntry(grchild)) {
		res = do_elem_btype(mod, grchild, grp);
	    }
	} else {
	    /* simple node */
	    res = do_elem_btype(mod, child, top);
	}
    }

    if (res != NO_ERR) {
	val_free_value(top);
    } else {
	val_add_child(top, val);
    }

    if (res == NO_ERR) {
	res = finish_attributes(mod, typdef, val);
    }

    return res;

}   /* finish_choice */


/********************************************************************
* FUNCTION do_elem_choice
* 
*   Generate the element node for a choice child type
*
* INPUTS:
*    mod == module conversion in progress
*    typch == typ_child_t for the child node to use
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->v.childQ has entries added for the child type
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    do_elem_choice (const ncx_module_t *mod,
		    const typ_child_t *typch,
		    val_value_t *val)
{
    val_value_t    *elem, *cpx;
    status_t        res;
    xmlns_id_t      xsd_id;

    /* check if this is a named type with a choice base type */
    if (typch->typdef.class == NCX_CL_NAMED) {
	elem = xsd_new_element(mod, typch->name, &typch->typdef, 
			       typch->parent, FALSE, FALSE);
	if (elem) {
	    val_add_child(elem, val);
	    return NO_ERR;
	} else {
	    return ERR_INTERNAL_MEM;
	}
    }

    xsd_id = xmlns_xs_id();

    /* element choice of N arbitrary nodes */
    elem = xsd_new_element(mod, typch->name, &typch->typdef, 
			   typch->parent, TRUE, FALSE);
    if (!elem) {
	return ERR_INTERNAL_MEM;
    }

    /* next level is complexType */
    cpx = xml_val_new_struct(XSD_CPX_TYP, xsd_id);
    if (!cpx) {
	val_free_value(elem);
	return ERR_INTERNAL_MEM;
    } else {
	val_add_child(cpx, elem);  /* add early */
    }

    res = finish_choice(mod, &typch->typdef, cpx);

    /* generate the attributes allowed in the struct node */
    if (res != NO_ERR) {
	val_free_value(elem);
    } else {
	val_add_child(elem, val);
    }

    return res;

}   /* do_elem_choice */


/********************************************************************
* FUNCTION finish_table
* 
*   Generate the XSD for a table data type
*
* INPUTS:
*    mod == module in progress
*    typdef == typ_def for the typ_template struct to use
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->v.childQ has entries added for this data type
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    finish_table (const ncx_module_t *mod,
		  const typ_def_t *typdef,
		  val_value_t *val)
{
    val_value_t          *seq;
    const typ_child_t    *child;
    const typ_index_t    *idx;
    status_t              res;
    xmlns_id_t            xsd_id;

    res = NO_ERR;
    xsd_id = xmlns_xs_id();

    /* next level is sequence */
    seq = xml_val_new_struct(XSD_SEQUENCE, xsd_id);
    if (!seq) {
	return ERR_INTERNAL_MEM;
    }

    /* go through all the index nodes and generate elements for them */
    for (idx = typ_first_index(&typdef->def.complex);
	 idx != NULL;
	 idx = typ_next_index(idx)) {

	switch (idx->ityp) {
	case NCX_IT_INLINE:
	case NCX_IT_NAMED:
	    res = do_elem_btype(mod, &idx->typch, seq);
	    break;
	case NCX_IT_LOCAL:
	case NCX_IT_SLOCAL:
	    /* skip these local index components; they will be
	     * handled later whild the child nodes are processed
	     */
	    res = NO_ERR;
	    break;
	case NCX_IT_REMOTE:   /* never used in typ_parse.c! */
	case NCX_IT_SREMOTE:
	    res = do_elem_btype(mod, &idx->typch, seq);
	    break;
	default:
	    val_free_value(seq);
	    return SET_ERROR(ERR_INTERNAL_VAL);
	}

	if (res != NO_ERR) {
	    val_free_value(seq);
	    return res;
	}
    }

    /* go through all the child nodes and generate elements for them */
    for (child = typ_first_child(&typdef->def.complex);
	 child != NULL && res==NO_ERR;
	 child = typ_next_con_child(child)) {
	res = do_elem_btype(mod, child, seq);
    }

    if (res != NO_ERR) {
	val_free_value(seq);
    } else {
	val_add_child(seq, val);
    }

    /* generate the attributes allowed in the table node */
    if (res == NO_ERR) {
	res = finish_attributes(mod, typdef, val);
    }

    return res;

}   /* finish_table */


/********************************************************************
* FUNCTION do_elem_table
* 
*   Generate the element node for a table child type
*
* INPUTS:
*    mod == module conversion in progress
*    typch == typ_child_t for the child node to use
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->v.childQ has entries added for the child type
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    do_elem_table (const ncx_module_t *mod,
		   const typ_child_t *typch,
		   val_value_t *val)
{
    val_value_t    *elem, *cpx;
    status_t        res;
    xmlns_id_t      xsd_id;

    /* check if this is a named type with a struct base type */
    if (typch->typdef.class == NCX_CL_NAMED) {
	/* there is some index so generate a key node */
	elem = xsd_new_element(mod, typch->name, &typch->typdef, 
			       typch->parent, FALSE, TRUE);
	if (!elem) {
	    return ERR_INTERNAL_MEM;
	} 

	res = xsd_add_key(typch->name, &typch->typdef, elem);
	if (res != NO_ERR) {
	    val_free_value(elem);
	    return res;
	}

	val_add_child(elem, val);
	return NO_ERR;
    }


    res = NO_ERR;
    xsd_id = xmlns_xs_id();

    /* element table of N arbitrary nodes */
    elem = xsd_new_element(mod, typch->name, &typch->typdef, 
			   typch->parent, TRUE, TRUE);
    if (!elem) {
	return ERR_INTERNAL_MEM;
    }

    /* next level is complexType */
    cpx = xml_val_new_struct(XSD_CPX_TYP, xsd_id);
    if (!cpx) {
	res = ERR_INTERNAL_MEM;
    } else {
	val_add_child(cpx, elem);  /* add early */
    }

    if (res == NO_ERR) {
	res = finish_table(mod, &typch->typdef, cpx);
    }

    /* check if a key node is needed */
    if (res==NO_ERR) {
	/* there is some index so generate a key node */
	res = xsd_add_key(typch->name, &typch->typdef, elem);
    }


    if (res != NO_ERR) {
	val_free_value(elem);
    } else {
	val_add_child(elem, val);
    }

    return res;

}   /* do_elem_table */


/********************************************************************
* FUNCTION do_elem_simple
* 
*   Generate the element node for a simple child type
*
* INPUTS:
*    mod == module conversion in progress
*    name == element name
*    typdef == typ_def_t for the node to use
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->v.childQ has entries added for the child type
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    do_elem_simple (const ncx_module_t *mod,
		    const xmlChar *name,
		    const typ_def_t *typdef,
		    val_value_t *val)
{
    val_value_t          *elem, *toptyp;
    const typ_simple_t   *simtyp;
    boolean               empty, simtop;
    status_t              res;

    empty = FALSE;
    simtop = TRUE;

    switch (typdef->class) {
    case NCX_CL_BASE:
	empty = TRUE;
	break;
    case NCX_CL_SIMPLE:
	simtyp = &typdef->def.simple;
	empty = (dlq_empty(&simtyp->rangeQ) &&
		 dlq_empty(&simtyp->unionQ) &&
		 dlq_empty(&simtyp->valQ) &&
		 dlq_empty(&simtyp->metaQ));
	break;
    case NCX_CL_COMPLEX:
	switch (typ_get_basetype(typdef)) {
	case NCX_BT_ANY:
	case NCX_BT_CONTAINER:
	    empty = TRUE;
	    simtop = FALSE;
	    break;
	default:
	    return SET_ERROR(ERR_INTERNAL_VAL);
	}
	break;
    case NCX_CL_NAMED:
	if (typdef->def.named.newtyp) {
	    simtyp = &typdef->def.named.newtyp->def.simple;
	    empty = (dlq_empty(&simtyp->rangeQ) &&
		     dlq_empty(&simtyp->valQ) &&
		     dlq_empty(&simtyp->metaQ));
	} else {
	    empty = TRUE;
	    switch (typ_get_basetype(typdef)) {
	    case NCX_BT_ANY:
	    case NCX_BT_CONTAINER:
		simtop = FALSE;
		break;
	    default:
		break;
	    }
	}
	break;
    default:
	return SET_ERROR(ERR_INTERNAL_VAL);
    }

    if (empty) {
	/* get an empty element */
	elem = xsd_new_element(mod, name, typdef, NULL, FALSE, FALSE);
	if (!elem) {
	    return ERR_INTERNAL_MEM;
	}
    } else {
	elem = xsd_new_element(mod, name, typdef, NULL, TRUE, FALSE);
	if (!elem) {
	    return ERR_INTERNAL_MEM;
	}

	if (simtop) {
	    toptyp = xml_val_new_struct(XSD_SIM_TYP, xmlns_xs_id());
	} else {
	    toptyp = xml_val_new_struct(XSD_CPX_TYP, xmlns_xs_id());
	}
	if (!toptyp) {
	    val_free_value(elem);
	    return ERR_INTERNAL_MEM;
	} else {
	    val_add_child(toptyp, elem);  /* add early */
	}

	switch (typdef->class) {
	case NCX_CL_SIMPLE:
	    res = xsd_finish_simpleType(mod, typdef, toptyp);
	    break;
	case NCX_CL_NAMED:
	    res = xsd_finish_namedType(mod, typdef, toptyp);
	    break;
	default:
	    res = SET_ERROR(ERR_INTERNAL_VAL);
	}
	if (res != NO_ERR) {
	    val_free_value(elem);
	    return res;
	}
    }

    val_add_child(elem, val);
    return NO_ERR;

}   /* do_elem_simple */


/********************************************************************
* FUNCTION do_elem_btype
* 
*   Generate the element node for any child type
*
* INPUTS:
*    mod == module conversion in progress
*    typch == typ_child_t for the child node to use
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->v.childQ has entries added for the child type
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    do_elem_btype (const ncx_module_t *mod,
		   const typ_child_t *typch,
		   val_value_t *val)
{
    switch (typ_get_basetype(&typch->typdef)) {
    case NCX_BT_ENUM:
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
    case NCX_BT_STRING:
    case NCX_BT_BINARY:
    case NCX_BT_INSTANCE_ID:
    case NCX_BT_ANY:
    case NCX_BT_EMPTY:
    case NCX_BT_BOOLEAN:
	return do_elem_simple(mod, typch->name, &typch->typdef, val);
    case NCX_BT_SLIST:
	return do_elem_list(mod, typch, val);
    case NCX_BT_UNION:
	return do_elem_union(mod, typch, val);
    case NCX_BT_CONTAINER:
	return do_elem_struct(mod, typch, val);
    case NCX_BT_CHOICE:
	return do_elem_choice(mod, typch, val);
    case NCX_BT_LIST:
	return do_elem_table(mod, typch, val);
    default:
	return SET_ERROR(ERR_INTERNAL_VAL);
    }
    /*NOTREACHED*/

}   /* do_elem_btype */


/********************************************************************
* FUNCTION finish_complexType
* 
*   Generate a complexType elment based on the typdef
*
* INPUTS:
*    mod == module in progress
*    typdef == typ def for the typ_template struct to convert
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->v.childQ has entries added for the types in this module
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    finish_complexType (const ncx_module_t *mod,
			const typ_def_t *typdef,
			val_value_t *val)
{
    val_value_t        *topcon, *ext;
    const typ_def_t    *realtypdef, *flagtypdef;
    xmlChar            *str;
    ncx_btype_t         btyp;
    status_t            res;
    xmlns_id_t          xsd_id;

    xsd_id = xmlns_xs_id();
    res = NO_ERR;
    btyp = typ_get_basetype(typdef);
    realtypdef = typ_get_cbase_typdef(typdef);
    flagtypdef = typ_get_basetype_typdef(NCX_BT_EMPTY);

    switch (btyp) {
    case NCX_BT_EMPTY:
	/* this type is represented as an empty element 
	 * so there is nothing to do but exit.  There cannot
	 * be any extensions, but there can be meta deta defined
	 */
	if (!typ_first_meta_con(typdef)) {
	    break;
	}
	/* else fall through and create some content */
    case NCX_BT_ANY:
	/* complexContent/extension of a pre-defined base type */
	topcon = xml_val_new_struct(XSD_CPX_CON, xsd_id);
	if (!topcon) {
	    return ERR_INTERNAL_MEM;
	}

	/* create an extension node */
	if (typ_first_meta_con(typdef)) {
	    ext = xml_val_new_struct(XSD_EXTENSION, xsd_id);
	} else {
	    ext = xml_val_new_flag(XSD_EXTENSION, xsd_id);
	}
	if (!ext) {
	    val_free_value(topcon);
	    return ERR_INTERNAL_MEM;
	} else {
	    val_add_child(ext, topcon);  /* add early */
	}

	/* add base=QName attribute */
	str = xml_val_make_qname(xsd_id, xsd_typename(btyp));
	if (!str) {
	    val_free_value(topcon);
	    return ERR_INTERNAL_MEM;
	}
	res = xml_val_add_cattr(XSD_BASE, 0, str, ext);
	if (res != NO_ERR) {
	    m__free(str);
	    val_free_value(topcon);
	    return ERR_INTERNAL_MEM;
	}
	
	/* add any attributes defined for this type */
	res = finish_attributes(mod, typdef, ext);
	if (res != NO_ERR) {
	    val_free_value(topcon);
	    return ERR_INTERNAL_MEM;
	}

	val_add_child(topcon, val);
	break;
    case NCX_BT_CONTAINER:
	res = finish_struct(mod, typdef, val);
	break;
    case NCX_BT_CHOICE:
	res = finish_choice(mod, typdef, val);
	break;
    case NCX_BT_LIST:
	res = finish_table(mod, typdef, val);
	break;
    default:
	res = SET_ERROR(ERR_INTERNAL_VAL);
    }

    return res;

}   /* finish_complexType */


/********************************************************************
* FUNCTION finish_simpleMetaType
* 
*   Generate a complexType element based on the typdef for a 
*   simple type that has attributes defined
*
* INPUTS:
*    mod == module in progress
*    typ == typ_template struct to convert to a simpleType
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->v.childQ has entries added for the types in this module
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    finish_simpleMetaType (const ncx_module_t *mod,
			   const typ_template_t *typ,
			   val_value_t *val)
{
    val_value_t *simcon, *ext;
    xmlns_id_t   xsd_id;
    xmlChar     *basename;
    status_t     res;

    xsd_id = xmlns_xs_id();

    simcon = xml_val_new_struct(XSD_SIM_CON, xsd_id);
    if (!simcon) {
	return ERR_INTERNAL_MEM;
    }

    ext = xml_val_new_struct(XSD_EXTENSION, xsd_id);
    if (!ext) {
	val_free_value(simcon);
	return ERR_INTERNAL_MEM;
    }
    val_add_child(ext, simcon);   /* add early */

    basename = xsd_make_basename(typ);
    if (!basename) {
	val_free_value(simcon);
	return ERR_INTERNAL_MEM;
    }

    /* Note: Since the targetNamespace is set to the default namespace
     * the basename is not converted to a QName here, since no prefix
     * will be added anyway
     */
    res = xml_val_add_attr(XSD_BASE, 0, basename, ext);
    if (res != NO_ERR) {
	m__free(basename);
	val_free_value(simcon);
	return ERR_INTERNAL_MEM;
    }

    /* convert all the metadata to attribute nodes */
    res = finish_attributes(mod, &typ->typdef, ext);
    if (res != NO_ERR) {
	val_free_value(simcon);
	return ERR_INTERNAL_MEM;
    }

    /* add the simpleContent node and exit */
    val_add_child(simcon, val); 
    return NO_ERR;

}   /* finish_simpleMetaType */


/************* E X T E R N A L   F U N C T I O N S *****************/


/********************************************************************
* FUNCTION xsd_add_types
* 
*   Add the required type nodes
*
* INPUTS:
*    mod == module in progress
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->childQ has entries added for the types in this module
*
* RETURNS:
*   status
*********************************************************************/
status_t
    xsd_add_types (const ncx_module_t *mod,
		   val_value_t *val)
{
    const typ_template_t   *typ;
    const typ_def_t        *typdef;
    val_value_t            *tval;
    xmlChar                *basename;
    xmlns_id_t              xsd_id;
    ncx_btype_t             btyp;
    ncx_tclass_t            tclass;
    status_t                res;
    boolean                 sim, base;

    xsd_id = xmlns_xs_id();

    for (typ = (const typ_template_t *)dlq_firstEntry(&mod->typeQ);
	 typ != NULL;
	 typ = (const typ_template_t *)dlq_nextEntry(typ)) {

	/* get type info */
	typdef = &typ->typdef;
	btyp = typ_get_basetype(typdef);
	tclass = typdef->class;

	/* A simple type that has attributes defined needs
	 * to be declared in a simpleType for the base without
         * any attributes, and a complexType that is an extension
	 * of the base type
	 */
	base = (typ_first_meta(&typ->typdef)) ? TRUE : FALSE;

	/* figure out if this is a complexType or a simpleType
	 * The ename and flag data types are simple in NCX but
	 * need to be complexTypes in XSD
	 */
	sim = typ_is_xsd_simple(btyp);

	if (sim) {
	    tval = xml_val_new_struct(XSD_SIM_TYP, xsd_id);
	} else {
	    tval = xml_val_new_struct(XSD_CPX_TYP, xsd_id);
	}
	if (!tval) {
	    return ERR_INTERNAL_MEM;
	}

	/* if this is a base type then 2 types will be generated
	 * 1) base type
	 * 2) extension of base type with attributes added
	 *
	 * First get the struct for the type
	 */
	if (base) {
	    basename = xsd_make_basename(typ);
	    if (!basename) {
		val_free_value(tval);
		return ERR_INTERNAL_MEM;
	    }
	    res = xml_val_add_attr(NCX_EL_NAME, 0, basename, tval);
	    if (res != NO_ERR) {
		m__free(basename);
	    }
	} else {
	    res = xml_val_add_cattr(NCX_EL_NAME, 0, typ->name, tval);
	}
	if (res != NO_ERR) {
	    val_free_value(tval);
	    return res;
	}

	if (base) {
	    /* create the base type */
	    if (tclass==NCX_CL_NAMED) {
		res = xsd_finish_namedType(mod, &typ->typdef, tval);
	    } else if (sim) {
		res = xsd_finish_simpleType(mod, &typ->typdef, tval);
	    }
	    if (res != NO_ERR) {
		val_free_value(tval);
		return res;
	    } else {
		val_add_child(tval, val);
	    }

	    /* start the real type, reuse tval */
	    tval = xml_val_new_struct(XSD_CPX_TYP, xsd_id);
	    if (!tval) {
		return ERR_INTERNAL_MEM;
	    }
	    res = xml_val_add_cattr(NCX_EL_NAME, 0, typ->name, tval);
	    if (res != NO_ERR) {
		val_free_value(tval);
		return res;
	    }
	}

	/* add an annotation node if there are any 
	 * 'extra' or appinfo clauses present
	 */
	res = xsd_do_type_annotation(typ, tval);
	if (res != NO_ERR) {
	    val_free_value(tval);
	    return res;
	}

	if (base) {
	    res = finish_simpleMetaType(mod, typ, tval);
	} else if (tclass==NCX_CL_NAMED) {
	    res = xsd_finish_namedType(mod, &typ->typdef, tval);
	} else if (sim) {
	    res = xsd_finish_simpleType(mod, &typ->typdef, tval);
	} else {
	    res = finish_complexType(mod, &typ->typdef, tval);
	}
	if (res != NO_ERR) {
	    val_free_value(tval);
	    return res;
	} else {
	    val_add_child(tval, val);
	}

    }

    return NO_ERR;

}   /* xsd_add_types */


/********************************************************************
* FUNCTION xsd_add_local_types
* 
*   Add the required type nodes for the local types in the typnameQ
*   YANG Only -- NCX does not have local types
*
* INPUTS:
*    mod == module in progress
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->childQ has entries added for the types in this module
*
* RETURNS:
*   status
*********************************************************************/
status_t
    xsd_add_local_types (const ncx_module_t *mod,
			 val_value_t *val)
{
    const ncx_typname_t    *typnam;
    const typ_template_t   *typ;
    const typ_def_t        *typdef;
    val_value_t            *tval;
    xmlns_id_t              xsd_id;
    ncx_btype_t             btyp;
    ncx_tclass_t            tclass;
    status_t                res;
    boolean                 sim;

    res = NO_ERR;
    xsd_id = xmlns_xs_id();

    for (typnam = (const ncx_typname_t *)dlq_firstEntry(&mod->typnameQ);
	 typnam != NULL;
	 typnam = (const ncx_typname_t *)dlq_nextEntry(typnam)) {

	/* get type info */
	typ = typnam->typ;
	typdef = &typ->typdef;
	btyp = typ_get_basetype(typdef);
	tclass = typdef->class;

	/* figure out if this is a complexType or a simpleType
	 * The ename and flag data types are simple in NCX but
	 * need to be complexTypes in XSD
	 */
	sim = typ_is_xsd_simple(btyp);

	if (sim) {
	    tval = xml_val_new_struct(XSD_SIM_TYP, xsd_id);
	} else {
	    tval = xml_val_new_struct(XSD_CPX_TYP, xsd_id);
	}
	if (!tval) {
	    return ERR_INTERNAL_MEM;
	} else {
	    val_add_child(tval, val);
	}

	res = xml_val_add_cattr(NCX_EL_NAME, 0, typnam->typname, tval);
	if (res != NO_ERR) {
	    return res;
	}

	/* add an annotation node if there are any 
	 * 'extra' or appinfo clauses present
	 */
	res = xsd_do_type_annotation(typ, tval);
	if (res != NO_ERR) {
	    return res;
	}

	if (tclass==NCX_CL_NAMED) {
	    res = xsd_finish_namedType(mod, &typ->typdef, tval);
	} else if (sim) {
	    res = xsd_finish_simpleType(mod, &typ->typdef, tval);
	} else {
	    res = finish_complexType(mod, &typ->typdef, tval);
	}
    }

    return res;

}  /* xsd_add_local_types */


/********************************************************************
* FUNCTION xsd_finish_simpleType
* 
*   Generate a simpleType elment based on the typdef
*   Note: NCX_BT_ENAME is considered a
*         complexType for XSD conversion purposes
*
* INPUTS:
*    mod == module in progress
*    typdef == typ def for the typ_template struct to convert
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->v.childQ has entries added for the types in this module
*
* RETURNS:
*   status
*********************************************************************/
status_t
    xsd_finish_simpleType (const ncx_module_t *mod,
			   const typ_def_t *typdef,
			   val_value_t *val)
{
    val_value_t      *topcon, *simtyp, *chval;
    const typ_sval_t *typ_sval;
    const typ_rangedef_t *typ_rdef;
    const xmlChar     *typename;
    const dlq_hdr_t   *rangeQ;
    uint32             rangecnt;
    xmlns_id_t         xsd_id;
    ncx_btype_t        btyp;
    status_t           res;

    xsd_id = xmlns_xs_id();
    btyp = typ_get_basetype(typdef);
    typename = xsd_typename(btyp);
    rangeQ = typ_get_crangeQ_con(typdef);
    rangecnt = (rangeQ) ? dlq_count(rangeQ) : 0;
    typ_rdef = typ_first_rangedef_con(typdef);
    res = NO_ERR;

    if (rangecnt > 1) {
	chval = new_range_union(mod, rangeQ, typdef, btyp,
				xsd_id, typename);
	if (!chval) {
	    return ERR_INTERNAL_MEM;
	} else {
	    val_add_child(chval, val);
	    return NO_ERR;
	}
    }

    switch (btyp) {
    case NCX_BT_ENUM:
	chval = make_enum(mod, typdef);
	if (!chval) {
	    return ERR_INTERNAL_MEM;
	} else {
	    /* add the restriction element to the simpleType */
	    val_add_child(chval, val);
	}
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
    case NCX_BT_BINARY:
    case NCX_BT_BOOLEAN:
    case NCX_BT_INSTANCE_ID:
	chval = new_restriction(mod, xsd_id, typename,
				    (rangecnt) ? TRUE : FALSE);
	if (!chval) {
	    return ERR_INTERNAL_MEM;
	} else {
	    val_add_child(chval, val);
	}
	if (rangecnt) {
	    res = finish_range(typdef, typ_rdef, chval);
	    if (res != NO_ERR) {
		return res;
	    }
	}
	break;
    case NCX_BT_STRING:
	switch (typdef->def.simple.strrest) {
	case NCX_SR_NONE:
	    /* build a restriction element */
	    chval = new_restriction(mod, xsd_id, typename,
				    (rangecnt) ? TRUE : FALSE);
	    if (!chval) {
		return ERR_INTERNAL_MEM;
	    } else {
		val_add_child(chval, val);
	    }
	    if (rangecnt) {
		res = finish_range(typdef, typ_rdef, chval);
		if (res != NO_ERR) {
		    return res;
		}
	    }
	    break;
	case NCX_SR_VALSET:
	    /* build a restriction element */
	    chval = new_restriction(mod, xsd_id, NCX_EL_STRING, TRUE);
	    if (!chval) {
		return ERR_INTERNAL_MEM;
	    } else {
		val_add_child(chval, val);
	    }

	    /* add all the value strings */
	    for (typ_sval = typ_first_strdef(typdef);
		 typ_sval != NULL;
		 typ_sval = (typ_sval_t *)dlq_nextEntry(typ_sval)) {
		res = add_sval_restriction(typ_sval->val, chval);
		if (res != NO_ERR) {
		    return res;
		}
	    }
	    break;
	case NCX_SR_PATTERN:
	    /* get the pattern first */
	    typ_sval = typ_first_strdef(typdef);
	    if (!typ_sval) {
		return SET_ERROR(ERR_INTERNAL_VAL);
	    }

	    /* build a restriction element */
	    chval = new_restriction(mod, xsd_id, NCX_EL_STRING, TRUE);
	    if (!chval) {
		return ERR_INTERNAL_MEM;
	    } else {
		val_add_child(chval, val);
	    }

	    /* create the pattern element */
	    res = add_pattern(typdef, typ_sval->val, chval);
	    if (res != NO_ERR) {
		return res;
	    }

	    if (rangecnt) {
		res = finish_range(typdef, typ_rdef, chval);
	    }
	    break;
	default:
	    return SET_ERROR(ERR_INTERNAL_VAL);
	}
	break;
    case NCX_BT_SLIST:
	res = finish_list(mod, btyp, typdef, val);
	break;
    case NCX_BT_UNION:
	res = xsd_finish_union(mod, typdef, val);
	break;
    case NCX_BT_KEYREF:
	break;
    case NCX_BT_BITS:
	/* a bits type is list of identifier strings */
	topcon = xml_val_new_struct(NCX_EL_LIST, xsd_id);
	if (!topcon) {
	    return ERR_INTERNAL_MEM;
	} else {
	    val_add_child(topcon, val);  /* add early */
	}

	/* add inline simpleType */
	simtyp = xml_val_new_struct(XSD_SIM_TYP, xsd_id);
	if (!simtyp) {
	    return ERR_INTERNAL_MEM;
	} else {
	    val_add_child(simtyp, topcon);  /* add early */
	}

	/* build an enumeration restriction */
	chval = make_enum(mod, typdef);
	if (!chval) {
	    return ERR_INTERNAL_MEM;
	} else {
	    val_add_child(chval, simtyp);
	}
	break;
    case NCX_BT_EMPTY:
	/* treat this as a string, max length is zero 
	 * !!!! this is not used at this time !!!!!
	 * !!!! the NCX_BT_EMPTY is a complexType !!!!
	 */
	topcon = new_restriction(mod, xsd_id, typename, TRUE);
	if (!topcon) {
	    return ERR_INTERNAL_MEM;
	} else {
	    val_add_child(topcon, val);
	}
	chval = xml_val_new_flag(XSD_MAX_LEN, xsd_id);
	if (!chval) {
	    return ERR_INTERNAL_MEM;
	} else {
	    val_add_child(chval, topcon);
	}
	res = xml_val_add_cattr(NCX_EL_VALUE, 0, XSD_ZERO, chval);
	break;
    default:
	return SET_ERROR(ERR_INTERNAL_VAL);
    }

    return res;

}   /* xsd_finish_simpleType */


/********************************************************************
* FUNCTION xsd_finish_namedType
* 
*   Generate a complexType elment based on the typdef for a named type
*   The tclass must be NCX_CL_NAMED for the typdef data struct
*
*   Note: The "+=" type extension mechanism in NCX is not supportable
*         in XSD for strings, so it must not be used at this time.
*         It is not supported for any data type, even enum.
*
* INPUTS:
*    mod == module in progress
*    typdef == typ def for the typ_template struct to convert
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->v.childQ has entries added for the types in this module
*
* RETURNS:
*   status
*********************************************************************/
status_t
    xsd_finish_namedType (const ncx_module_t *mod,
			  const typ_def_t *typdef,
			  val_value_t *val)
{
    val_value_t          *topcon, *chval;
    const typ_child_t    *mdef;
    const dlq_hdr_t      *rangeQ;
    const typ_rangedef_t *rdef;
    const typ_sval_t     *sdef;
    xmlChar              *str;
    ncx_btype_t           btyp;
    status_t              res;
    xmlns_id_t            xsd_id, test_id;
    boolean               issim, isext, hasnodes;

    /* init local vars */
    res = NO_ERR;
    xsd_id = xmlns_xs_id();
    btyp = typ_get_basetype(typdef);            /* NCX base type */
    mdef = typ_first_meta_con(typdef);            /* attribute Q */
    rdef = typ_first_rangedef_con(typdef);        /* range def Q */
    sdef = typ_first_strdef(typdef);         /* string/pattern Q */
    issim = typ_is_xsd_simple(btyp);           /* container type */
    test_id = typdef->def.named.typ->nsid;     /* NS of the name */
    if (!test_id) {
	test_id = mod->nsid;
    }

    /* check if this is simpleContent or complexContent */
    if (!issim) {
	topcon = xml_val_new_struct(XSD_CPX_CON, xsd_id);
	if (!topcon) {
	    return ERR_INTERNAL_MEM;
	} else {
	    val_add_child(topcon, val);      /* add early */
	}
    } else {
	/* simpleType does not have a simpleContent container
	 * so use the top-level 'val' node instead
	 */
	topcon = val;   
    }

    /* check if this is a restriction or an extension from the base type 
     * if a range def exists (rdef) then an extension is needed;
     * attributes require a complexType container;
     *
     * Only the semantics defined in the particular type
     * are examined, and just the extensions or restrictions
     * from the named parent type
     */
    switch (btyp) {
    case NCX_BT_BOOLEAN:
    case NCX_BT_INSTANCE_ID:
	isext = FALSE;
	hasnodes = (mdef) ? TRUE : FALSE;
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
	isext = FALSE;
	hasnodes = (mdef || rdef) ? TRUE : FALSE;	
	break;
    case NCX_BT_STRING:
    case NCX_BT_BINARY:
	isext = FALSE;
	hasnodes = (rdef || sdef || mdef) ? TRUE : FALSE;
	break;
    case NCX_BT_ENUM:
	hasnodes = (sdef || mdef) ? TRUE : FALSE;
	isext = hasnodes;
	break;
    case NCX_BT_UNION:
    case NCX_BT_ANY:
    case NCX_BT_EMPTY:
    case NCX_BT_SLIST:
    case NCX_BT_CONTAINER:
    case NCX_BT_CHOICE:
    case NCX_BT_LIST:
	hasnodes = (mdef) ? TRUE : FALSE;
	isext = hasnodes;
	break;
    case NCX_BT_KEYREF:
	isext = FALSE;
	hasnodes = TRUE;
	break;
    default:
	res = SET_ERROR(ERR_INTERNAL_VAL);
	return res;
    }

    /* check if this is a restriction type with a multi-part range */
    if (!isext && rdef) {
	rangeQ = typ_get_crangeQ_con(typdef);
	if (dlq_count(rangeQ) > 1) {
	    chval = new_range_union(mod, rangeQ, typdef,
				    btyp, test_id,
				    typdef->def.named.typ->name);
	    if (!chval) {
		return ERR_INTERNAL_MEM;
	    } else {
		val_add_child(chval, topcon);
		if (!issim) {
		    val_add_child(topcon, val);
		}
		return NO_ERR;
	    }
	}
    }

    /* create an extension or restriction */
    if (isext) {
	if (hasnodes) {
	    chval = xml_val_new_struct(XSD_EXTENSION, xsd_id);
	} else {
	    chval = xml_val_new_flag(XSD_EXTENSION, xsd_id);
	}
    } else {
	if (hasnodes) {
	    chval = xml_val_new_struct(XSD_RESTRICTION, xsd_id);
	} else {
	    chval = xml_val_new_flag(XSD_RESTRICTION, xsd_id);
	}
    }
    if (!chval) {
	return ERR_INTERNAL_MEM;
    } else {
	val_add_child(chval, topcon);  /* add early */
    }

    /* add the base=QName attribute */
    if (test_id == mod->nsid) {
	/* named type is a top-level definition */
	res = xml_val_add_cattr(XSD_BASE, 0, 
		       typdef->def.named.typ->name, chval);
	if (res != NO_ERR) {
	    return ERR_INTERNAL_MEM;
	}
    } else if (!test_id) {
	/* named type is a local type */
	res = xml_val_add_cattr
	    (XSD_BASE, 0, 
	     ncx_find_typname(typdef->def.named.typ, &mod->typnameQ),
	     chval);
	if (res != NO_ERR) {
	    return ERR_INTERNAL_MEM;
	}
    } else {
	/* add base=QName attribute */
	str = xml_val_make_qname(test_id, typdef->def.named.typ->name);
	if (!str) {
	    return ERR_INTERNAL_MEM;
	}
	res = xml_val_add_attr(XSD_BASE, 0, str, chval);
	if (res != NO_ERR) {
	    m__free(str);
	    return ERR_INTERNAL_MEM;
	}
    }

    /* Note: At this time, the simple type extensions via the 
     * "+=" operator are not supported.  I can't figure out how
     * to extend simple content, like adding enums and adding
     * additional patterns to check in a logical OR expression
     * and I don't want to cut-and-paste all the ancestor values
     * instead of using a type extension
     */
    if (hasnodes && typdef->def.named.newtyp) {
	/* check if this is an range restriction */
	if (!isext && rdef) {
	    res = finish_range(typdef, rdef, chval);
	}

	/* check if this is an attribute extension */
	if (res==NO_ERR && isext && mdef) {
	    /* add any attributes defined for this type */
	    res = finish_attributes(mod, typdef->def.named.newtyp, chval);
	} 
    }

    return res;

}   /* xsd_finish_namedType */


/********************************************************************
* FUNCTION xsd_finish_union
* 
*   Generate the XSD for a union element 
*
* INPUTS:
*    mod == module in progress
*    typdef == typdef for the list
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->v.childQ has entries added for the nodes in this typdef
*
* RETURNS:
*   status
*********************************************************************/
status_t
    xsd_finish_union (const ncx_module_t *mod,
		      const typ_def_t *typdef,
		      val_value_t *val)
{
    const typ_unionnode_t *un;
    const xmlChar        *typname;
    val_value_t          *top, *simtyp;
    const typ_def_t      *untypdef;
    xmlChar              *str, *p;
    uint32                len, siz, perline;
    xmlns_id_t            union_id, xsd_id;
    status_t              res;
    boolean               simple_form;

    res = NO_ERR;
    len = 0;
    perline = PER_LINE;
    simple_form = TRUE;
    xsd_id = xmlns_xs_id();

    un = typ_first_unionnode(typdef);
    if (!un) {
	return SET_ERROR(ERR_INTERNAL_VAL);
    }

    /* determine the length of memberTypes attribute string */
    while (un && simple_form) {
	if (un->typ) {
	    /* the union member is an NCX named type only
	     * or a YANG named type clause
	     */
	    union_id = typ_get_nsid(un->typ);
	    if (!union_id) {
		union_id = mod->nsid;
	    }
	    typname = un->typ->name;

	    if (union_id != mod->nsid) {	
		len += xml_val_qname_len(union_id, typname) + 1;
	    } else {
		len += xml_strlen(typname) + 1;
	    }
	    un = (typ_unionnode_t *)dlq_nextEntry(un);
	} else {
	    /* the union member is an unnamed YANG type clause */
	    simple_form = FALSE;
	}
    }

    if (simple_form) {
	/* simple form with memberTypes=list of 'QName' */
	top = xml_val_new_flag(XSD_UNION, xsd_id);
	if (!top) {
	    return ERR_INTERNAL_MEM;
	}

	/* get the string buffer */
	str = m__getMem(++len);
	if (!str) {
	    val_free_value(top);
	    return ERR_INTERNAL_MEM;
	}

	/* reset the union type list pointer */
	un = typ_first_unionnode(typdef);
	p = str;

	/* construct the memberTypes attribute value string */
	while (un) {
	    union_id = typ_get_nsid(un->typ);
	    if (!union_id) {
		union_id = mod->nsid;
	    }
	    typname = un->typ->name;

	    if (union_id != mod->nsid) {	
		siz = xml_val_sprintf_qname(p, len, union_id, typname);
	    } else {
		siz = xml_strcpy(p, typname);
	    }
	    p += siz;

	    if (!--perline) {
		*p++ = '\n';
		perline = PER_LINE;
	    } else {
		*p++ = ' ';
	    }
	    len -= siz+1;
	    un = (typ_unionnode_t *)dlq_nextEntry(un);
	}

	/* undo the last (extra) space */
	*--p = 0;

	/* add the itemType attribute to the list element */
	res = xml_val_add_attr(XSD_MEMBERTYPES, 0, str, top);
	if (res != NO_ERR) {
	    m__free(str);
	    val_free_value(top);
	    return res;
	}
    } else {
	/* at least one unnamed type within the union
	 * so all member types have to be in the inline form
	 */
	top = xml_val_new_struct(XSD_UNION, xsd_id);
	if (!top) {
	    return ERR_INTERNAL_MEM;
	}

	/* reset the union type list pointer */
	un = typ_first_unionnode(typdef);
	while (un && res==NO_ERR) {
	    simtyp = xml_val_new_struct(XSD_SIM_TYP, xsd_id);
	    if (!simtyp) {
		val_free_value(top);
		return ERR_INTERNAL_MEM;
	    } else {
		val_add_child(simtyp, top);  /* add early */
	    }

	    untypdef = un->typdef;

	    if (untypdef->class == NCX_CL_NAMED) {
		res = xsd_finish_namedType(mod, untypdef, simtyp);
	    } else {
		res = xsd_finish_simpleType(mod, untypdef, simtyp);
	    }
	    if (res == NO_ERR) {
		un = (typ_unionnode_t *)dlq_nextEntry(un);
	    }
	}

	if (res != NO_ERR) {
	    val_free_value(top);
	    return res;
	}
    }

    val_add_child(top, val);
    return NO_ERR;

}   /* xsd_finish_union */


/* END file xsd_typ.c */

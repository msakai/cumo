/*
  ndloop.c
  Numerical Array Extension for Ruby
    (C) Copyright 1999-2011 by Masahiro TANAKA

  This program is free software.
  You can distribute/modify this program
  under the same terms as Ruby itself.
  NO WARRANTY.
*/

//#define NARRAY_C
#include <ruby.h>
#include "narray.h"


#ifdef HAVE_STDARG_PROTOTYPES
#include <stdarg.h>
#define va_init_list(a,b) va_start(a,b)
#else
#include <varargs.h>
#define va_init_list(a,b) va_start(a)
#endif

#if 0
static void
print_ndfunc(ndfunc_t *nf) {
    int i;
    printf("ndfunc_t = 0x%"SZF"x {\n",(size_t)nf);
    printf("  func  = 0x%"SZF"x\n", (size_t)nf->func);
    printf("  flag  = 0x%"SZF"x\n", (size_t)nf->flag);
    printf("  narg  = %d\n", nf->nin);
    printf("  nopt  = %d\n", nf->nopt);
    printf("  nres  = %d\n", nf->nout);
    printf("  args  = 0x%"SZF"x\n", (size_t)nf->ain);
    for (i=0; i<nf->nin+nf->nout; i++) {
        printf("  args[%d].type = 0x%"SZF"x\n", i, (size_t)nf->ain[i].type);
        printf("  args[%d].init = 0x%"SZF"x\n", i, (size_t)nf->ain[i].init);
        printf("  args[%d].dim = %d\n", i, nf->ain[i].dim);
        printf("  args[%d].aux.shape[0] = %"SZF"u\n", i, nf->ain[i].aux.shape[0]);
    }
    printf("}\n");
}


static void
print_ndloop(na_md_loop_t *lp) {
    int i,j,nd;
    printf("na_md_loop_t = 0x%"SZF"x {\n",(size_t)lp);
    printf("  narg = %d\n", lp->narg);
    printf("  ndim = %d\n", lp->ndim);
    printf("  n = 0x%"SZF"x\n", (size_t)lp->n);
    printf("  args = 0x%"SZF"x\n", (size_t)lp->args);
    printf("  iter = 0x%"SZF"x\n", (size_t)lp->iter);
    printf("  user.narg = %d\n", lp->user.narg);
    printf("  user.ndim = %d\n", lp->user.ndim);
    printf("  user.n = 0x%"SZF"x\n", (size_t)lp->user.n);
    printf("  user.args = 0x%"SZF"x\n", (size_t)lp->user.args);
    printf("  user.iter = 0x%"SZF"x\n", (size_t)lp->user.iter);
    printf("  user.info = 0x%"SZF"x\n", (size_t)lp->user.info);
    printf("  user.opt_ptr = 0x%"SZF"x\n", (size_t)lp->user.opt_ptr);
    if (lp->reduce==Qnil) {
        printf("  reduce  = nil\n");
    } else {
        printf("  reduce  = 0x%x\n", NUM2INT(lp->reduce));
    }
    nd = lp->ndim + lp->user.ndim;
    for (i=0; i<=nd; i++) {
        printf("  n[%d] = %"SZF"u\n", i, lp->n[i]);
    }
    for (j=0; j<lp->narg; j++) {
        printf("  args[%d].ptr = 0x%"SZF"x\n", j, (size_t)lp->args[j].ptr);
        printf("  args[%d].elmsz = %"SZF"d\n", j, lp->args[j].elmsz);
        printf("  args[%d].value = 0x%"SZF"x\n", j, lp->args[j].value);
        for (i=0; i<=nd; i++) {
            printf("  iter[%d,%d].pos = %"SZF"u\n", i,j, LITER(lp,i,j).pos);
            printf("  iter[%d,%d].step = %"SZF"u\n", i,j, LITER(lp,i,j).step);
            printf("  iter[%d,%d].idx = 0x%"SZF"x\n", i,j, (size_t)LITER(lp,i,j).idx);
        }
    }
    printf("}\n");
}
#endif


static inline VALUE
nary_type_s_cast(VALUE type, VALUE obj)
{
    return rb_funcall(type,rb_intern("cast"),1,obj);
}




static VALUE
ndloop_get_arg_type(ndfunc_t *nf, VALUE args, int j)
{
    int i;
    VALUE t;

    // type
    t = nf->aout[j].type;
    // if type is FIXNUM, get the type of i-th argument
    if (FIXNUM_P(t)) {
        i = FIX2INT(t);
        if (i<0 || i>=nf->nin) {
            rb_bug("invalid type: index (%d) out of # of args",i);
        }
        t = nf->ain[i].type;
        // if i-th type is Qnil, get the type of i-th input value
        if (t==Qnil) {
            t = CLASS_OF(RARRAY_PTR(args)[i]);
        }
    }
    return t;
}



// cast argument : RARRAY_PTR(args)[j] to type : nf->args[j].type
static void
ndloop_cast_args(ndfunc_t *nf, VALUE args)
{
    int j;
    char *s;
    volatile VALUE v, t, x;

    if (na_debug_flag) rb_p(args);

    for (j=0; j<nf->nin; j++) {
        t = nf->ain[j].type;

        // argument
        v = RARRAY_PTR(args)[j];
        // skip cast if type is nil or same as input value
        if (RTEST(t) && t != CLASS_OF(v)) {
            // else do cast
            if (rb_obj_is_kind_of(t, rb_cClass)) {
                if (RTEST(rb_class_inherited_p(t, cNArray))) {
                    v = nary_type_s_cast(t, v);
                    RARRAY_PTR(args)[j] = v;
                    continue;
                }
            }
            x = rb_inspect(t);
            s = StringValueCStr(x);
            rb_bug("fail cast from %s to %s", rb_obj_classname(v),s);
            rb_raise(rb_eTypeError,"fail cast from %s to %s",
                     rb_obj_classname(v), s);
        }
    }
}


/*
  user-dimension:
    user_nd = MAX( nf->args[j].dim )

  user-support dimension:

  loop dimension:
    loop_nd
*/

//static na_md_loop_t *
static VALUE
ndloop_alloc(ndfunc_t *nf, VALUE args, void *opt_ptr)
{
    int i,j;
    int narg;
    na_md_loop_t *lp;
    int user_nd, loop_nd, max_nd, tmp_nd;
    VALUE v, t;
    narray_t *na;
    size_t sz1, sz2, sz3, sz4;
    char *ptr;

    if (RARRAY_LEN(args) != nf->nin) {
        rb_bug("wrong number of arguments for ndfunc (%"SZF"u for %d)",
               RARRAY_LEN(args), nf->nin);
    }

    // find max dimension
    user_nd = 0;
    loop_nd = 0;
    for (j=0; j<nf->nin; j++) {
        // max dimension of user function
        tmp_nd = nf->ain[j].dim;
        if (tmp_nd > user_nd) {
            user_nd = tmp_nd;
        }
        // max dimension of md-loop
        v = RARRAY_PTR(args)[j];
        if (IsNArray(v)) {
            GetNArray(v,na);
            // array-dimension minus user-dimension
            tmp_nd = na->ndim - nf->ain[j].dim;
            if (tmp_nd > loop_nd) {
                loop_nd = tmp_nd;
            }
        }
    }

    narg = nf->nin + nf->nout;
    max_nd = loop_nd + user_nd;

    //lp = ALLOC(na_md_loop_t);
    //lp->n    = ALLOC_N(size_t,(max_nd+1));
    //lp->args = ALLOC_N(na_loop_args_t,narg+1);
    //lp->iter = ALLOC_N(na_loop_iter_t,narg*(max_nd+1));

    sz1 = sizeof(na_md_loop_t);
    sz2 = sizeof(size_t)*(max_nd+1);
    sz3 = sizeof(na_loop_args_t)*(narg+1);
    sz4 = sizeof(na_loop_iter_t)*(narg*(max_nd+1));
    ptr = ALLOC_N(char,sz1+sz2+sz3+sz4);
    lp       = (na_md_loop_t*)ptr;   ptr += sz1;
    lp->n    = (size_t*)ptr;         ptr += sz2;
    lp->args = (na_loop_args_t*)ptr; ptr += sz3;
    lp->iter = (na_loop_iter_t*)ptr;

    lp->vargs = args;
    lp->ndfunc = nf;

    lp->narg = narg;
    lp->user.narg = narg;
    lp->ndim = loop_nd;
    lp->user.ndim = user_nd;
    lp->user.args = lp->args;

    for (i=0; i<=max_nd; i++) {
        lp->n[i] = 1;
        for (j=0; j<narg; j++) {
            LITER(lp,i,j).pos = 0;
            LITER(lp,i,j).step = 0;
            LITER(lp,i,j).idx = NULL;
        }
    }

    // options
    lp->reduce = Qnil;
    lp->user.info = Qnil;
//    lp->user.opt_ptr = opt_ptr;
//
//    for (i=0; i<nf->nopt; i++) {
//        t = nf->opt_types[i];
//        if (t==sym_reduce) {
//            lp->reduce = RARRAY_PTR(args)[i+nf->nin];
//        }
//        /*
//        else if (t==sym_transpose) {
//            lp->transpose = RARRAY_PTR(args)[i+nf->nin];
//        }
//        */
//        else if (t==sym_info) {
//            lp->user.info = RARRAY_PTR(args)[i+nf->nin];
//        }
//        else {
//            fprintf(stderr,"ndloop parse_options: unknown type");
//        }
//    }

    //return lp;
    return Data_Wrap_Struct(rb_cData,0,0,lp);
}


static VALUE
ndloop_release(VALUE vlp)
{
    int j;
    VALUE v;
    na_md_loop_t* lp;
    Data_Get_Struct(vlp,na_md_loop_t,lp);

    for (j=0; j < lp->narg; j++) {
        v = lp->args[j].value;
        if (IsNArray(v)) {
            na_release_lock(v);
        }
    }
    return Qnil;
}


static void
ndloop_free(na_md_loop_t* lp)
{
    int j;
    VALUE v;

    for (j=0; j<lp->narg; j++) {
        v = lp->args[j].value;
        if (IsNArray(v)) {
            na_release_lock(v);
        }
    }
    //xfree(lp->iter);
    //xfree(lp->args);
    //xfree(lp->n);
    xfree(lp);
}



static void
ndloop_check_shape(na_md_loop_t *lp, int nf_dim, narray_t *na)
{
    int i, k;
    size_t n;
    int dim_beg;

    dim_beg = lp->ndim + nf_dim - na->ndim;

    for (k=na->ndim-1; k>=0; k--) {
        i = k + dim_beg;
        n = na->shape[k];
        // if n==1 then repeat this dimension
        if (n>1) {
            if (lp->n[i] == 1) {
                lp->n[i] = n;
            } else if (lp->n[i] != n) {
                // inconsistent array shape
                rb_raise(rb_eTypeError,"shape1[%d](=%"SZF"u) != shape2[%d](=%"SZF"u)",
                         i, lp->n[i], k, n);
            }
        }
    }
}


/*
na->shape[i] == lp->n[ dim_map[i] ]
 */
static void
ndloop_set_stepidx(na_md_loop_t *lp, int j, narray_t *na, int *dim_map)
{
    size_t n, s;
    int i, k;
    stridx_t sdx;

    switch(NA_TYPE(na)) {
    case NARRAY_DATA_T:
        if (NA_DATA_PTR(na)==NULL && NA_SIZE(na)>0) {
            rb_bug("cannot read no-data NArray");
            rb_raise(rb_eRuntimeError,"cannot read no-data NArray");
        }
        // through
    case NARRAY_FILEMAP_T:
        s = lp->args[j].elmsz;
        for (k=na->ndim; k--;) {
            i = dim_map[k];
            n = na->shape[k];
            if (n > 1) {
                LITER(lp,i,j).step = s;
                s *= n;
            }
            LITER(lp,i,j).idx = NULL;
        }
        break;
    case NARRAY_VIEW_T:
        for (k=0; k<na->ndim; k++) {
            i = dim_map[k];
            n = na->shape[k];
            if (n > 1) {
                sdx = NA_VIEW_STRIDX(na)[k];
                if (SDX_IS_INDEX(sdx)) {
                    LITER(lp,i,j).step = 0;
                    LITER(lp,i,j).idx = SDX_GET_INDEX(sdx);
                } else {
                    LITER(lp,i,j).step = SDX_GET_STRIDE(sdx);
                    LITER(lp,i,j).idx = NULL;
                }
            }
        }
        LITER(lp,0,j).pos = NA_VIEW_OFFSET(na);
        break;
    default:
        rb_raise(rb_eRuntimeError,"invalid narray internal type");
    }
}


static VALUE
ndloop_set_narray_result(ndfunc_t *nf, na_md_loop_t *lp, int j,
                         VALUE type, VALUE args, VALUE results)
{
    int i, jj;
    int na_ndim;
    volatile VALUE v;
    size_t *na_shape;
    int *dim_map;
    narray_t *na;

    int max_nd = lp->ndim + lp->user.ndim;

    na_shape = ALLOCA_N(size_t, max_nd);
    dim_map = ALLOCA_N(int, max_nd);

    // md-loop shape
    na_ndim = 0;
    for (i=0; i<lp->ndim; i++) {
        if (na_test_reduce(lp->reduce,i)) {        // accumulate dimension
            if (NDF_TEST(nf,NDF_KEEP_DIM)) {
                na_shape[na_ndim] = 1;         // leave it
            } else {
                continue;  // delete dimension
            }
        } else {
            na_shape[na_ndim] = lp->n[i];
        }
        dim_map[na_ndim++] = i;
    }

    // user-specified shape
//    if (nf->aout[j].dim==1) {
//        na_shape[na_ndim] = nf->aout[j].shape[0];
//        dim_map[na_ndim++] = lp->ndim;
//    } else if (nf->aout[j].dim>1) {
        for (i=0; i<nf->aout[j].dim; i++) {
            na_shape[na_ndim] = nf->aout[j].shape[i];
            dim_map[na_ndim++] = i + lp->ndim;
        }
//    }

    // in-place check
    for (jj=0; jj<nf->nin; jj++) {
        v = RARRAY_PTR(args)[jj];
        if (TEST_INPLACE(v)) {
            // type check
            if (type != CLASS_OF(v))
                goto not_in_place;
            // already used for result ?
            for (i=0; i<RARRAY_LEN(results); i++) {
                if (v == RARRAY_PTR(results)[i])
                    goto not_in_place;
            }
            GetNArray(v,na);
            // shape check
            if (na->ndim == na_ndim) {
                for (i=0; i<na_ndim; i++) {
                    if (na_shape[i] != na->shape[i])
                        goto not_in_place;
                }
                goto in_place;
            }
        }
    not_in_place:
        ;
    }

    // new object
    v = rb_narray_new(type, na_ndim, na_shape);

 in_place:
    if (nf->aout[j].init != Qnil) {
        if (na_debug_flag) rb_p(v);
        na_store(v, nf->aout[j].init);
    }

    GetNArray(v,na);
    lp->args[j].value = v;
    lp->args[j].elmsz = na_get_elmsz(v);
    lp->args[j].ptr   = na_get_pointer_for_write(v);

    ndloop_set_stepidx(lp, j, na, dim_map);

    return v;
}



static VALUE
ndloop_init_args(ndfunc_t *nf, na_md_loop_t *lp, VALUE args)
{
    int i, j, k;
    volatile VALUE v, t, results;
    narray_t *na;
    int nall = nf->nin + nf->nout;
    int nf_dim;
    int dim_beg;
    int *dim_map;

    int max_nd = lp->ndim + lp->user.ndim;

    dim_map = ALLOCA_N(int, max_nd);

    // input arguments
    for (j=0; j<nf->nin; j++) {
        //printf("j=%d\n",j);
        v = RARRAY_PTR(args)[j];
        if (IsNArray(v)) {
            // set lp->args[j] with v
            //ndloop_set_narray_arg(v,j,lp,nf);
            GetNArray(v,na);
            lp->args[j].value = v;
            lp->args[j].elmsz = na_get_elmsz(v);
            lp->args[j].ptr   = na_get_pointer_for_write(v); // read
            // OK during reading, BLOCK during writing
            nf_dim = nf->ain[j].dim;
            ndloop_check_shape(lp, nf_dim, na);
            dim_beg = lp->ndim + nf->ain[j].dim - na->ndim;
            for (i=0; i<na->ndim; i++) {
                dim_map[i] = i+dim_beg;
            }
            ndloop_set_stepidx(lp, j, na, dim_map);
            //puts("pass313");
        } else if (TYPE(v)==T_ARRAY) {
            lp->args[j].value = v;
            lp->args[j].elmsz = sizeof(VALUE);
            lp->args[j].ptr   = NULL;
            for (i=0; i<=max_nd; i++) {
                //printf("i=%d, j=%d\n",i,j);
                LITER(lp,i,j).pos = 0;
                LITER(lp,i,j).step = 1;
                LITER(lp,i,j).idx = NULL;
            }
        }
    }


    // output results
    results = rb_ary_new2(nf->nout);

    //for (j=nf->nin; j<nall; j++) {
    for (k=0; k<nf->nout; k++) {
        //t = nf->args[j].type;
        t = ndloop_get_arg_type(nf,args,k);

        if (rb_obj_is_kind_of(t, rb_cClass)) {
            if (RTEST(rb_class_inherited_p(t, cNArray))) {
                // NArray
                v = ndloop_set_narray_result(nf,lp,k,t,args,results);
                rb_ary_push(results, v);
            }
            else if (RTEST(rb_class_inherited_p(t, rb_cArray))) {
                j = nf->nin + k;
                // Ruby Array
                for (i=0; i<=max_nd; i++) {
                    //printf("i=%d, j=%d\n",i,j);
                    LITER(lp,i,j).pos = 0;
                    LITER(lp,i,j).step = sizeof(VALUE);
                    LITER(lp,i,j).idx = NULL;
                }
                lp->args[j].value = t;
                lp->args[j].elmsz = sizeof(VALUE);
            } else {
                rb_raise(rb_eRuntimeError,"ndloop_init_args: invalid for type");
            }
        }
    }

    return results;
}



// return 1 if loop for one call of user function
// return 0 if single step for one call of user function
// Fix me for the case of user function supports more than 2 dimension
static int
//ndfunc_loop_able_in_userfunc(ndfunc_t *nf, na_md_loop_t *lp)
ndfunc_check_user_loop(ndfunc_t *nf, na_md_loop_t *lp)
{
    int j;
    int nargs = nf->nin + nf->nout;
    int i = lp->ndim;

    // If the user function supports STRIDE
    if (NDF_TEST(nf,NDF_STRIDE_LOOP)) {

        // If the user function supports both STRIDE and INDEX
        if (NDF_TEST(nf,NDF_INDEX_LOOP)) {
            // user function always does loop
            return 1;
        }

        // If the user function supports STRIDE but not INDEX
        else {
            // check array-index at the last one dimension
            for (j=0; j<nargs; j++) {
                if (LITER(lp,i,j).idx) {
                    return 0;
                }
            }
            // no index in argument array
            return 1;
        }
    }
    // If the user function supports only CONTIGUOUS loop
    else if (NDF_TEST(nf,NDF_CONTIGUOUS_LOOP)) {

        // Check argument ARRAYs
        for (j=0; j<nargs; j++) {
            // with INDEX?
            if (LITER(lp,i,j).idx)
                return 0;
            // with STRIDE?
            if (LITER(lp,i,j).step != lp->args[j].elmsz)
                return 0;
        }
        // All the argument ARRAYs are CONTIGUOUS
        return 1;
    }
    return 0;
}


static void
ndfunc_set_user_loop(ndfunc_t *nf, na_md_loop_t *lp)
{
    int dim;
    dim = ndfunc_check_user_loop(nf, lp);

    // loop in user function
    if (dim>0 && lp->ndim >= dim) {
        lp->user.ndim += dim;
        lp->ndim -= dim;
    }

    //printf("lp->ndim=%d &(lp->n[lp->ndim])=0x%x\n",lp->ndim,&(lp->n[lp->ndim]));
    lp->user.n = &(lp->n[lp->ndim]);
    lp->user.iter = &LITER(lp,lp->ndim,0);
}


// ---------------------------------------------------------------------------

static void
loop_narray(ndfunc_t *nf, na_md_loop_t *lp)
{
    size_t *c;
    int  i, j;
    int  nargs = lp->narg;
    int  nd = lp->ndim;

    if (nd<0) {
        rb_bug("bug? lp->ndim = %d\n", lp->ndim);
    }

    if (nd==0) {
        (*(nf->func))(&(lp->user));
        return;
    }

    // alloc counter
    c = ALLOCA_N(size_t, nd+1);
    for (i=0; i<=nd; i++) c[i]=0;

    // loop body
    for (i=0;;) {
        for (; i<nd; i++) {
            for (j=0; j<nargs; j++) {
                //printf("i=%d,j=%d\n",i,j);
                if (LITER(lp,i,j).idx) {
                    LITER(lp,i+1,j).pos = LITER(lp,i,j).pos + LITER(lp,i,j).idx[c[i]];
                } else {
                    LITER(lp,i+1,j).pos = LITER(lp,i,j).pos + LITER(lp,i,j).step*c[i];
                }
            }
        }
        //for (j=0; j<nargs; j++) printf("LITER(lp,i,j).pos=%d i=%d j=%d\n",LITER(lp,i,j).pos,i,j);

        (*(nf->func))(&(lp->user));

        for (;;) {
            if (i<=0) goto loop_end;
            i--;
            if (++c[i] < lp->n[i]) break;
            c[i] = 0;
        }
    }
 loop_end:
    ;
    //xfree(c);
}


static VALUE
ndloop_run(VALUE vlp)
{
    volatile VALUE args, results;
    na_md_loop_t *lp;
    ndfunc_t *nf;

    Data_Get_Struct(vlp,na_md_loop_t,lp);
    args = lp->vargs;
    nf = lp->ndfunc;

    // setup ndloop iterator with arguments
    results = ndloop_init_args(nf, lp, args);

    // setup objects in which resuts are stored
    ndfunc_set_user_loop(nf, lp);

    if (na_debug_flag) print_ndloop(lp);

    // loop
    loop_narray(nf, lp);

    // extract result objects
    switch(nf->nout) {
    case 0:
        return Qnil;
    case 1:
        return RARRAY_PTR(results)[0];
    }

    // free ndloop structure
    //ndloop_free(lp);
    return results;
}


VALUE
ndloop_do_main(ndfunc_t *nf, VALUE args, void *opt_ptr)
{
    volatile VALUE vlp;

    //rb_p(args);
    if (na_debug_flag) print_ndfunc(nf);

    // cast arguments to NArray
    ndloop_cast_args(nf, args);

    // allocate ndloop struct
    vlp = ndloop_alloc(nf, args, opt_ptr);

    return rb_ensure(ndloop_run, vlp, ndloop_release, vlp);
}


VALUE
#ifdef HAVE_STDARG_PROTOTYPES
ndloop_do(ndfunc_t *nf, int argc, ...)
#else
ndloop_do(nf, argc, va_alist)
  ndfunc_t *nf;
  int argc;
  va_dcl
#endif
{
    va_list ar;

    int i;
    VALUE *argv;
    volatile VALUE args;

    argv = ALLOCA_N(VALUE,argc);

    va_init_list(ar, argc);
    for (i=0; i<argc; i++) {
        argv[i] = va_arg(ar, VALUE);
    }
    va_end(ar);

    args = rb_ary_new4(argc, argv);

    return ndloop_do_main(nf, args, NULL);
}

<% (is_float ? ["_ignan","_prnan"] : [""]).each do |j|
   [64,32].each do |i| %>
#define idx_t int<%=i%>_t
static void
<%=type_name%>_index<%=i%>_qsort<%=j%>(cumo_na_loop_t *const lp)
{
    size_t   i, n, idx;
    char    *d_ptr, *i_ptr, *o_ptr;
    ssize_t  d_step, i_step, o_step;
    char   **ptr;

    CUMO_INIT_COUNTER(lp, n);
    CUMO_INIT_PTR(lp, 0, d_ptr, d_step);
    CUMO_INIT_PTR(lp, 1, i_ptr, i_step);
    CUMO_INIT_PTR(lp, 2, o_ptr, o_step);

    ptr = (char**)(lp->opt_ptr);

    //printf("(ptr=%lx, d_ptr=%lx,d_step=%ld, i_ptr=%lx,i_step=%ld, o_ptr=%lx,o_step=%ld)\n",(size_t)ptr,(size_t)d_ptr,(ssize_t)d_step,(size_t)i_ptr,(ssize_t)i_step,(size_t)o_ptr,(ssize_t)o_step);

    if (n==1) {
        *(idx_t*)o_ptr = *(idx_t*)(i_ptr);
        return;
    }

    for (i=0; i<n; i++) {
        ptr[i] = d_ptr + d_step * i;
        //printf("(%ld,%.3f)",i,*(double*)ptr[i]);
    }

    <%=type_name%>_index_qsort<%=j%>(ptr, n, sizeof(dtype*));

    //d_ptr = lp->args[0].ptr;
    //printf("(d_ptr=%lx)\n",(size_t)d_ptr);

    for (i=0; i<n; i++) {
        idx = (ptr[i] - d_ptr) / d_step;
        *(idx_t*)o_ptr = *(idx_t*)(i_ptr + i_step * idx);
        //printf("(idx[%ld]=%ld,%d)",i,idx,*(idx_t*)o_ptr);
        o_ptr += o_step;
    }
    //printf("\n");
}
#undef idx_t
<% end;end %>

/*
  <%=name%>. Returns an index array of sort result.
<% if is_float %>
  @overload <%=name%>(axis:nil, nan:false)
  @param [TrueClass] nan  If true, propagete NaN. If false, ignore NaN.
<% else %>
  @overload <%=name%>(axis:nil)
<% end %>
  @param [Numeric,Array,Range] axis  Affected dimensions.
  @return [Integer,Cumo::Int] returns result index of <%=name%>.
  @example
      Cumo::NArray[3,4,1,2].sort_index => Cumo::Int32[2,3,0,1]
*/
static VALUE
<%=c_func(-1)%>(int argc, VALUE *argv, VALUE self)
{
    size_t size;
    cumo_narray_t *na;
    VALUE idx, tmp, reduce, res;
    char *buf;
    cumo_ndfunc_arg_in_t ain[3] = {{cT,0},{0,0},{cumo_sym_reduce,0}};
    cumo_ndfunc_arg_out_t aout[1] = {{0,0,0}};
    cumo_ndfunc_t ndf = {0, CUMO_STRIDE_LOOP_NIP|CUMO_NDF_FLAT_REDUCE|CUMO_NDF_CUM, 3,1, ain,aout};

    CumoGetNArray(self,na);
    if (na->ndim==0) {
        return INT2FIX(0);
    }
    if (na->size > (~(u_int32_t)0)) {
        ain[1].type =
        aout[0].type = cumo_cInt64;
        idx = cumo_na_new(cumo_cInt64, na->ndim, na->shape);
       <% if is_float %>
         ndf.func = <%=type_name%>_index64_qsort_ignan;
         reduce = cumo_na_reduce_dimension(argc, argv, 1, &self, &ndf,
                                      <%=type_name%>_index64_qsort_prnan);
       <% else %>
         ndf.func = <%=type_name%>_index64_qsort;
         reduce = cumo_na_reduce_dimension(argc, argv, 1, &self, &ndf, 0);
       <% end %>
    } else {
        ain[1].type =
        aout[0].type = cumo_cInt32;
        idx = cumo_na_new(cumo_cInt32, na->ndim, na->shape);
       <% if is_float %>
         ndf.func = <%=type_name%>_index32_qsort_ignan;
         reduce = cumo_na_reduce_dimension(argc, argv, 1, &self, &ndf,
                                      <%=type_name%>_index32_qsort_prnan);
       <% else %>
         ndf.func = <%=type_name%>_index32_qsort;
         reduce = cumo_na_reduce_dimension(argc, argv, 1, &self, &ndf, 0);
       <% end %>
    }
    rb_funcall(idx, rb_intern("seq"), 0);

    size = na->size*sizeof(void*); // max capa
    buf = rb_alloc_tmp_buffer(&tmp, size);

    CUMO_SHOW_SYNCHRONIZE_FIXME_WARNING_ONCE("<%=name%>", "<%=type_name%>");
    cudaDeviceSynchronize();

    res = cumo_na_ndloop3(&ndf, buf, 3, self, idx, reduce);
    rb_free_tmp_buffer(&tmp);
    return res;
}

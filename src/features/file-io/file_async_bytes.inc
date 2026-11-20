/* Managed byte arrays have eight-byte scalar slots; native buffers are packed.
 * Charge each element against the shared conversion budget as eight bytes. */
static void io_prepare_bytes(AsyncTask *t,uint64_t *budget)
{
 FileAsyncJob *j=t->io_job;uint64_t i,count=j->length-t->io_offset;
 uint64_t *data=(uint64_t *)t->io_lines[3];
 if(count>*budget/8)count=*budget/8;
 for(i=0;i<count;i++)j->data[t->io_offset+i]=(unsigned char)data[t->io_offset+i];
 t->io_offset+=count;*budget-=count*8;
}
static void io_read_bytes(AsyncRuntime *r,AsyncTask *t,uint64_t *budget)
{
 FileAsyncJob *j=t->io_job;uint64_t i,count,*data;
 if(j->length>(INT64_MAX-32)/8){t->error=io_error(r,t,0,13,7);io_dispose(t);finish(t);return;}
 if(!t->io_result)t->io_result=array_new(r,j->length,t->io_shape);
 data=(uint64_t *)t->io_result[3];count=j->length-t->io_offset;
 if(count>*budget/8)count=*budget/8;
 for(i=0;i<count;i++)data[t->io_offset+i]=j->data[t->io_offset+i];
 t->io_offset+=count;*budget-=count*8;
 if(t->io_offset==j->length){t->result=(uint64_t)t->io_result;io_dispose(t);finish(t);}
}

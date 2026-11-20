/* Managed-side streaming of captured lines. Every scan/copy shares the byte
 * budget and the fixed worker-slot rotation with text result conversion. */
static void io_prepare_lines(AsyncTask *t,uint64_t *budget)
{
 FileAsyncJob *j=t->io_job;
 while(*budget&&t->io_offset<j->length){
  uint64_t *line=(uint64_t *)((uint64_t *)t->io_lines[3])[t->io_line_index];
  uint64_t *part=t->io_part?t->io_separator:line;
  uint64_t length=t->io_part&&!(t->io_final||t->io_line_index+1<t->io_lines[0])?0:part[0];
  uint64_t count=length-t->io_part_offset,i;if(count>*budget)count=*budget;
  for(i=0;i<count;i++)j->data[t->io_offset+i]=((unsigned char *)(part+1))[t->io_part_offset+i];
  t->io_offset+=count;t->io_part_offset+=count;*budget-=count;
  if(t->io_part_offset==length){
   t->io_part_offset=0;
   if(t->io_part){t->io_part=0;t->io_line_index++;}else t->io_part=1;
  }
 }
}
static int io_line_count(AsyncRuntime *r,AsyncTask *t)
{
 if(t->io_count>=t->io_max_lines){t->error=io_error(r,t,0,13,8);io_dispose(t);finish(t);return 0;}
 t->io_count++;return 1;
}
static void io_read_lines(AsyncRuntime *r,AsyncTask *t,uint64_t *budget)
{
 uint64_t length=t->io_result[0];const unsigned char *text=(const unsigned char *)(t->io_result+1);
 if(t->io_phase==1){
  while(*budget&&t->io_offset<length){
   unsigned char c=text[t->io_offset++];(*budget)--;
   if(c=='\r'||c=='\n'){
    if((c=='\r'||!t->io_last_cr)&&!io_line_count(r,t))return;
    t->io_line_start=t->io_offset;
   }
   t->io_last_cr=c=='\r';
  }
  if(t->io_offset!=length)return;
  if(t->io_line_start<length&&!io_line_count(r,t))return;
  t->io_lines=array_new(r,t->io_count,t->io_shape);t->io_phase=2;t->io_offset=t->io_line_start=0;
 }
 while(*budget){
  if(t->io_phase==2){
   if(t->io_offset==length&&t->io_line_start==length){t->result=(uint64_t)t->io_lines;io_dispose(t);finish(t);return;}
   while(*budget&&t->io_offset<length&&text[t->io_offset]!='\r'&&text[t->io_offset]!='\n'){t->io_offset++;(*budget)--;}
   if(t->io_offset<length&&text[t->io_offset]!='\r'&&text[t->io_offset]!='\n')return;
   t->io_line_end=t->io_offset;t->io_part_offset=t->io_offset;
   if(t->io_part_offset<length){
    t->io_part_offset++;
    if(text[t->io_offset]=='\r'&&t->io_part_offset<length&&text[t->io_part_offset]=='\n')t->io_part_offset++;
   }
   t->io_line=r->allocate(r,t->io_line_end-t->io_line_start+8);t->io_line[0]=t->io_line_end-t->io_line_start;
   t->io_offset=0;t->io_phase=3;if(*budget)(*budget)--;
  }
  if(t->io_phase==3){
   uint64_t count=t->io_line[0]-t->io_offset,i;if(count>*budget)count=*budget;
   for(i=0;i<count;i++)((unsigned char *)(t->io_line+1))[t->io_offset+i]=text[t->io_line_start+t->io_offset+i];
   t->io_offset+=count;*budget-=count;
   if(t->io_offset!=t->io_line[0])return;
   ((uint64_t *)t->io_lines[3])[t->io_line_index++]=(uint64_t)t->io_line;t->io_line=0;
   t->io_offset=t->io_line_start=t->io_part_offset;t->io_phase=2;
  }
 }
}

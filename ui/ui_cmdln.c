#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "ui/ui_const.h"
#include "ui/ui_prompt.h"
#include "ui/ui_args.h"
#include "ui/ui_cmdln.h"


// the command line struct with buffer and several pointers
struct _command_line cmdln; //everything the user entered before <enter>
struct _command_info_t command_info; //the current command and position in the buffer
static const struct prompt_result empty_result;

void cmdln_init(void)
{
	for(uint32_t i=0; i<UI_CMDBUFFSIZE; i++) cmdln.buf[i]=0x00;
	cmdln.wptr=0;
    cmdln.rptr=0;
    cmdln.histptr=0;
    cmdln.cursptr=0;  
}
//pointer update, rolls over
uint32_t cmdln_pu(uint32_t i)
{
    return ((i)&(UI_CMDBUFFSIZE-1));
}

void cmdln_get_command_pointer(struct _command_pointer *cp)
{
    cp->wptr=cmdln.wptr;
    cp->rptr=cmdln.rptr;
}

bool cmdln_try_add(char *c)
{
    //TODO: leave one space for 0x00 command seperator????
    if(cmdln_pu(cmdln.wptr+1)==cmdln_pu(cmdln.rptr)) 
    {
        return false;
    }
    cmdln.buf[cmdln.wptr]=(*c); 
    cmdln.wptr=cmdln_pu(cmdln.wptr+1);
    return true;
}

bool cmdln_try_remove(char *c)
{
    if(cmdln_pu(cmdln.rptr) == cmdln_pu(cmdln.wptr))
    {
        return false;
    }

    (*c)=cmdln.buf[cmdln.rptr]; 
    cmdln.rptr=cmdln_pu(cmdln.rptr+1);
    return true;
}

bool cmdln_try_peek(uint32_t i, char *c)
{
    if(cmdln_pu(cmdln.rptr+i)==cmdln_pu(cmdln.wptr))
    {
        return false;
    }

    (*c)=cmdln.buf[cmdln_pu(cmdln.rptr+i)]; 
    
    if((*c)==0x00) return false;

    return true;
}

bool cmdln_try_peek_pointer(struct _command_pointer *cp, uint32_t i, char *c)
{
    if(cmdln_pu(cp->rptr+i)==cmdln_pu(cp->wptr))
    {
        return false;
    }

    (*c)=cmdln.buf[cmdln_pu(cp->rptr+i)]; 
    return true;
}

bool cmdln_try_discard(uint32_t i)
{
    //this isn't very effective, maybe just not use it??
    //if(cmdln_pu(cmdln.rptr+i) == cmdln_pu(cmdln.wprt))
    //{
     //   return false;
    //}
    cmdln.rptr=cmdln_pu(cmdln.rptr+i);
    return true;
}

bool cmdln_next_buf_pos(void)
{
    cmdln.rptr=cmdln.wptr; 
    cmdln.cursptr=cmdln.wptr; 
    cmdln.histptr=0;
}

//These are new functions to ease argument and options parsing
// Isolate the next command between the current read pointer and 0x00, (?test?) end of pointer, ; || && (next commmand)
// functions to move within the single command range

uint32_t cmdln_get_length_pointer(struct _command_line *cp)
{
    if(cp->rptr>cp->wptr)
    {
        return (UI_CMDBUFFSIZE - cp->rptr)+cp->wptr;
    }
    else
    {
        return cp->wptr - cp->rptr;
    }
}
/*
void ui_args_consume_whitespace(struct _command_pointer *cp)
{
    uint32_t bytes_remaining=cp->command_end-cp->command_ptr;
    cmdln_get_length_pointer(struct _command_pointer *cp)    
    for(uint32_t i=0; i<bytes_remaining; i++)
    {
        char c;
        if( (!cmdln_try_peek_pointer(&cp, cp->command_ptr, &c)) || c==0x00 || (c!=0x20) )
        {
            return;
        }
        cp->command_ptr++;
}*/
#if 0
struct command_pointer cp;

bool cmdln_find_next_command(void)
{
    //todo: consume white space
    for(uint32_t i=0; i<cp.length; i++) //scan through for || && ; and process part by part
    {
        char c,d;
        bool got_pos1=cmdln_try_peek_pointer(&cp, i, &c);
        bool got_pos2=cmdln_try_peek_pointer(&cp, i+1, &d);
        if(!got_pos1 || c==0x00 || c==';' || 
            (got_pos2 && ((c=='|' && d=='|') || (c=='&' && d=='&'))) 
        ){
            cp.command_end=i;
            break;
        }
    }
}
#endif

//consume white space (0x20, space)
// non_white_space = true, consume non-white space characters (not space)
bool cmdln_consume_white_space(uint32_t *rptr, bool non_white_space)
{
    //consume white space
    while(true)
    {
        char c;
        //no more characters
        if(!(command_info.endptr>=(command_info.startptr+(*rptr)) && cmdln_try_peek(command_info.startptr+(*rptr), &c)))
        {
            return false;
        }
        if( (!non_white_space && c==' ')//consume white space
            ||(non_white_space && c!=' ')){ //consume non-whitespace
            //printf("Whitespace at %d\r\n", cp->startptr+rptr);  
            (*rptr)++;
        }else{
            break;
        }
    }
    return true;
}

// internal function to take copy string from start position to next space or end of buffer
bool cmdln_args_get_string(uint32_t start_pos, uint32_t max_len, char *string)
{
    char c;
    uint32_t rptr=start_pos;

    //printf("rptr=%d\r\n)", rptr);

    for(uint32_t i=0; i<max_len; i++)
    {
        //no more characters
        if((!(command_info.endptr>=command_info.startptr+rptr && cmdln_try_peek(command_info.startptr+rptr, &c)))
            || c==' ' || i==(max_len-1))
        {
            string[i]=0x00;
            if(i==0) return false;
            else return true;
        }
        string[i]=c;
        rptr++;
    }
    
}

bool cmdln_args_string_by_position(uint32_t pos, uint32_t max_len, char *str)
{
    char c;
    uint32_t rptr=0;
    memset(str, 0x00, max_len);
    //start at beginning of command range
    #ifdef UI_CMDLN_ARGS_DEBUG
    printf("Looking for string in pos %d\r\n", pos);
    #endif
    for(uint32_t i=0; i<pos+1; i++)
    {
        //consume white space
        if(!cmdln_consume_white_space(&rptr, false))
        {
            return false;
        }
        //consume non-white space
        uint32_t charcnt=0;
        while(true)
        {
            //no more characters
            if(!(command_info.endptr>=command_info.startptr+rptr && cmdln_try_peek(command_info.startptr+rptr, &c)))
            {
                // is this the position we're after?
                // do we have any characters??
                if(i==pos && charcnt>0){
                    #ifdef UI_CMDLN_ARGS_DEBUG
                    printf("Found pos %d\r\n", i);
                    #endif
                    str[charcnt]=0x00;
                    return true;
                }else{                
                    return false;
                }
            }

            //not a space so consume it
            if(c!=' '){
                if((i==pos) && charcnt<(max_len-1))
                {
                    //printf("Char at %d: %c\r\n", cp->startptr+rptr, c);
                    str[charcnt]=c;
                    charcnt++;
                }
                rptr++;
            }else{ //next character is a space, we're done
                // is this the position we're after?
                if(i==pos)
                {
                    #ifdef UI_CMDLN_ARGS_DEBUG
                    printf("Found pos %d\r\n", i);
                    #endif
                    str[charcnt]=0x00;
                    return true;
                }                    
                break;
            }

        }

    }
    return false;
}


bool cmdln_args_find_arg(char flag, command_var_t *arg)
{
    uint32_t rptr=0;
    char flag_c, dash_c, space_c;
    arg->error=false;
    arg->has_arg=false;
    while(command_info.endptr>=(command_info.startptr+rptr+2) 
        && cmdln_try_peek(rptr, &dash_c) 
        && cmdln_try_peek(rptr+1, &flag_c)
        && cmdln_try_peek(rptr+2, &space_c))
    {
        if(dash_c=='-' && flag_c==flag && space_c==' ')
        {
            arg->has_arg=true;
            rptr=rptr+2;
            if( (!cmdln_consume_white_space(&rptr, false)) //end of buffer, no value
                || (cmdln_try_peek(rptr, &dash_c) && dash_c=='-')) //next argument, no value
            { 
                //printf("No value for flag %c\r\n", flag);
                arg->has_value=false;
                return true;
            } 
            //printf("Value for flag %c\r\n", flag);
            arg->has_value=true; 
            arg->value_pos=rptr;
            return true; 
        }
        rptr++;
    }
    //printf("Flag %c not found\r\n", flag);
    return false;
}



//check if a flag is present and get the string value
// returns true if flag is present AND has a string value
// check arg to see if a flag was present with no string value
bool cmdln_args_find_flag_string(char flag, command_var_t *arg, uint32_t max_len, char *value)
{
    if(!cmdln_args_find_arg(flag, arg)) return false;

    if(arg->has_value)
    {
        if(!cmdln_args_get_string(arg->value_pos, max_len, value))
        {
            arg->error=true;
            return false;
        }
    }
    return true;
}

// check if a -f(lag) is present. Value is don't care.
// returns true if flag is present
bool cmdln_find_flag(char flag, command_var_t *arg)
{
    if(!cmdln_args_find_arg(flag, arg)) return false;
    return true;
}


/*
bool ui_args_find_uint32(arg_var_t *arg, uint32_t *value)
{
    struct prompt_result result;
    uint32_t i=0;
    char c;
    arg->error=false;
    arg->has_value=false;

    //the read pointer should be at the end of the command
    //next we consume 1 or more spaces,
    //then copy text to the buffer until space or - or 0x00
    while(cmdln_try_peek(i, &c))
    {
        if(c=='-'||c==0x00)
        {
            return false;
        }

        if(c!=' ')
        {
            if(ui_args_get_int(i, &result, value))
            {
                arg->has_value=true;
                arg->value_pos=i;
                return true;
            }
            else
            {
                return false;
            }
        }

        i++;
    }
    return false;
}
*/
/*
bool ui_args_get_hex(struct prompt_result *result, uint32_t *value)
{
    char c;

    *result=empty_result;
    result->no_value=true;
    (*value)=0;

    while(cmdln_try_peek(0,&c)) //peek at next char
    {
        if(((c>='0')&&(c<='9')) )
        {
            (*value)<<=4;
            (*value)+=(c-0x30);
        }
        else if( ((c|0x20)>='a') && ((c|0x20)<='f') )
        {
            (*value)<<=4;
            c|=0x20;		// to lowercase
            (*value)+=(c-0x57);	// 0x61 ('a') -0xa
        }
        else
        {
            return false;
        }
        cmdln_try_discard(1);//discard
        result->success=true;
        result->no_value=false;  
    }

    return result->success;
}

bool ui_args_get_bin(struct prompt_result *result, uint32_t *value)
{
    char c;
    *result=empty_result;
    result->no_value=true;
    (*value)=0;

    while(cmdln_try_peek(0,&c)) //peek at next char
    {
        if( (c<'0')||(c>'1') )
        {
            return false;
        }
        (*value)<<=1;
        (*value)+=c-0x30;
        cmdln_try_discard(1);//discard
        result->success=true;
        result->no_value=false;   
    }

    return result->success;    
}
*/
bool cmdln_args_get_dec(uint32_t *rptr, struct prompt_result *result, uint32_t *value)
{
    char c;
    *result=empty_result;    
    result->no_value=true; 
    (*value)=0;

    while(cmdln_try_peek(command_info.startptr+(*rptr),&c)) //peek at next char
    {
        if( (c<'0') || (c>'9') ) //if there is a char, and it is in range
        {
            break;
        }
        (*value)*=10;
        (*value)+=(c-0x30);    
        (*rptr)++;          
        result->success=true;
        result->no_value=false;
    }
    return result->success;
}

// decodes value from the cmdline
// XXXXXX integer
// 0xXXXX hexadecimal
// 0bXXXX bin
bool cmdln_args_get_int(uint32_t *rptr, struct prompt_result *result, uint32_t *value)
{
    bool r1,r2;
    char p1,p2;

    *result=empty_result;
    r1=cmdln_try_peek(command_info.startptr+(*rptr),&p1);
    r2=cmdln_try_peek(command_info.startptr+(*rptr)+1,&p2);

    if( !r1 || (p1==0x00) )// no data, end of data, or no value entered on prompt
    {
        result->no_value=true;
        return false;
    }

    if( r2 && (p2|0x20)=='x') // HEX
    {
        //cmdln_try_discard(2); //remove 0x
        (rptr)+=2;
        //ui_args_get_hex(result, value);
        result->number_format=df_hex;// whatever from ui_const
    }
    else if( r2 && (p2|0x20)=='b' ) // BIN
    {
        //cmdln_try_discard(2); //remove 0b
        (rptr)+=2;
        //ui_args_get_bin(result, value);
        result->number_format=df_bin;// whatever from ui_const        
    }
    else  // DEC
    {   
        cmdln_args_get_dec(rptr, result, value);
        result->number_format=df_dec;// whatever from ui_const        
    }
	return result->success;
}


bool cmdln_args_uint32_by_position(uint32_t pos, uint32_t *value)
{
    char c;
    uint32_t rptr=0;
    //start at beginning of command range
    #ifdef UI_CMDLN_ARGS_DEBUG
    printf("Looking for uint in pos %d\r\n", pos);
    #endif
    for(uint32_t i=0; i<pos+1; i++){
        //consume white space
        if(!cmdln_consume_white_space(&rptr, false)){
            return false;
        }
        //consume non-white space
        if(i!=pos){
            if(!cmdln_consume_white_space(&rptr, true)){ //consume non-white space
                return false;
            }
        }else{
            struct prompt_result result;
            if(cmdln_args_get_int(&rptr, &result, value)){
                return true;
            }else{
                return false;
            }
        }
    }
    return false;
}



// hand a blank struct or the previous struct to get the next command
bool cmdln_find_next_command(struct _command_info_t *cp)
{
    cp->startptr = cp->endptr = cp->nextptr; //should be zero on first call, use previous value for subsequent calls
    uint32_t i = 0;

    char c,d;
    if(!cmdln_try_peek(cp->endptr, &c))
    {
        #ifdef UI_CMDLN_ARGS_DEBUG
        printf("End of command line input at %d\r\n", cp->endptr);
        #endif
        cp->delimiter=false; //0 = end of command input
        return false;
    }
    memset(cp->command, 0x00, 9);
    while(true)
    {
        bool got_pos1=cmdln_try_peek(cp->endptr, &c);
        bool got_pos2=cmdln_try_peek(cp->endptr+1, &d);
        if(!got_pos1)
        {
            #ifdef UI_CMDLN_ARGS_DEBUG
            printf("Last/only command line input: pos1=%d, c=%d\r\n", got_pos1, c);
            #endif
            cp->delimiter=false; //0 = end of command input
            cp->nextptr=cp->endptr;
            goto cmdln_find_next_command_success;
        }
        else if(c==';')
        {
            #ifdef UI_CMDLN_ARGS_DEBUG
            printf("Got end of command: ; position: %d, \r\n", cp->endptr);
            #endif
            cp->delimiter=';';
            cp->nextptr=cp->endptr+1;
            goto cmdln_find_next_command_success;
        }
        else if(got_pos2 && ((c=='|' && d=='|')||(c=='&' && d=='&')))
        {
            #ifdef UI_CMDLN_ARGS_DEBUG
            printf("Got end of command: %c position: %d, \r\n", c, cp->endptr);
            #endif
            cp->delimiter=c;
            cp->nextptr=cp->endptr+2;
            goto cmdln_find_next_command_success;
        }else if(i<8)
        {
            cp->command[i]=c;
            if(c==' ') i=8; //stop at space if possible
            i++;
        }
        cp->endptr++;
    }
cmdln_find_next_command_success:
    cp->endptr--;    
    command_info.startptr=cp->startptr;
    command_info.endptr = cp->endptr;
    return true;
}


//function for debugging the command line arguments parsers
// shows all commands and all detected positions
bool cmdln_info(void)
{
    //start and end point?
    printf("Input start: %d, end %d\r\n",cmdln.rptr, cmdln.wptr);
    // how many characters?
    printf("Input length: %d\r\n", cmdln_get_length_pointer(&cmdln));
    uint32_t i=0;
    struct _command_info_t cp;
    cp.nextptr=0;
    while(cmdln_find_next_command(&cp))
    {
        printf("Command: %s, delimiter: %c\r\n", cp.command, cp.delimiter);
        uint32_t pos=0;
        char str[9];
        while(cmdln_args_string_by_position(pos, 9, str))
        {
            printf("String pos: %d, value: %s\r\n", pos, str);
            pos++;
        }
    }
}

//function for debugging the command line arguments parsers
// shows all commands and all detected positions
bool cmdln_info_uint32(void)
{
    //start and end point?
    printf("Input start: %d, end %d\r\n",cmdln.rptr, cmdln.wptr);
    // how many characters?
    printf("Input length: %d\r\n", cmdln_get_length_pointer(&cmdln));
    uint32_t i=0;
    struct _command_info_t cp;
    cp.nextptr=0;
    while(cmdln_find_next_command(&cp))
    {
        printf("Command: %s, delimiter: %c\r\n", cp.command, cp.delimiter);
        uint32_t pos=0;
        uint32_t value=0;
        while(cmdln_args_uint32_by_position(pos, &value))
        {
            printf("String pos: %d, value: %d\r\n", pos, value);
            pos++;
        }
    }
}
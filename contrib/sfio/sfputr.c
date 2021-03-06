#include	"sfhdr.h"

/*	Put out a null-terminated string
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
ssize_t sfputr(reg Sfio_t* f, const char* s, reg int rc)
#else
ssize_t sfputr(f,s,rc)
reg Sfio_t*	f;	/* write to this stream	*/
char*		s;	/* string to write	*/
reg int		rc;	/* record separator.	*/
#endif
{
	reg ssize_t	p, n, w;
	reg uchar*	ps;

	if(f->mode != SF_WRITE && _sfmode(f,SF_WRITE,0) < 0)
		return -1;

	SFLOCK(f,0);

	for(w = 0; (*s || rc >= 0); )
	{	SFWPEEK(f,ps,p);

		if(p == 0 || (f->flags&SF_WHOLE) )
		{	n = strlen(s);
			if(p >= (n + (rc < 0 ? 0 : 1)) )
			{	/* buffer can hold everything */
				if(n > 0)
				{	memcpy(ps, s, n);
					ps += n;
					w += n;
				}
				if(rc >= 0)
				{	*ps++ = rc;
					w += 1;
				}
				f->next = ps;
			}
			else
			{	/* create a reserve buffer to hold data */
				Sfrsrv_t*	frs;

				p = n + (rc >= 0 ? 1 : 0);
				if(!(frs = _sfrsrv(f, p)) )
					n = 0;
				else
				{	if(n > 0)
						memcpy(frs->data, s, n);
					if(rc >= 0)
						frs->data[n] = rc;
					if((n = SFWRITE(f,frs->data,p)) < 0 )
						n = 0;
				}

				w += n;
			}
			break;
		}

		if(*s == 0)
		{	*ps++ = rc;
			f->next = ps;
			w += 1;
			break;
		}

#if _lib_memccpy
		if((ps = (uchar*)memccpy(ps,s,'\0',p)) != NIL(uchar*))
			ps -= 1;
		else	ps  = f->next+p;
		s += ps - f->next;
#else
		for(; p > 0; --p, ++ps, ++s)
			if((*ps = *s) == 0)
				break;
#endif
		w += ps - f->next;
		f->next = ps;
	}

	/* sync unseekable shared streams */
	if(f->extent < 0 && (f->flags&SF_SHARE) )
		(void)SFFLSBUF(f,-1);

	/* check for line buffering */
	else if((f->flags&SF_LINE) && !(f->flags&SF_STRING) && (n = f->next-f->data) > 0)
	{	if(n > w)
			n = w;
		f->next -= n;
		(void)SFWRITE(f,(Void_t*)f->next,n);
	}

	SFOPEN(f,0);
	return w;
}

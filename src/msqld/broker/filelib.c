/*
** Copyright (c) 1995-2001  Hughes Technologies Pty Ltd.  All rights
** reserved.  
**
** Terms under which this software may be used or copied are
** provided in the  specific license associated with this product.
**
** Hughes Technologies disclaims all warranties with regard to this 
** software, including all implied warranties of merchantability and 
** fitness, in no event shall Hughes Technologies be liable for any 
** special, indirect or consequential damages or any damages whatsoever 
** resulting from loss of use, data or profits, whether in an action of 
** contract, negligence or other tortious action, arising out of or in 
** connection with the use or performance of this software.
**
**
** $Id: filelib.c,v 1.3 2011/03/30 06:13:31 bambi Exp $
**
*/

/*
** Module	: broker : filelib
** Purpose	: file descriptor passing routines
** Exports	: 
** Depends Upon	: 
*/



/**************************************************************************
** STANDARD INCLUDES
**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <common/config.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#endif
#include <errno.h>


/**************************************************************************
** MODULE SPECIFIC INCLUDES
**************************************************************************/

#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/param.h>
#include <common/portability.h>
#include <common/msql_defs.h>
#include <common/debug/debug.h>


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/


/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/


/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/


#if !defined(OLD_BSD_MSG) && !defined(NEW_BSD_MSG) && !defined(SYSV_MSG) && !defined(LINUX_MSG)

    #######  Must defined one of the known FD passing methods  ########

#endif


/************************************************************************
** File descriptor passing routines for the old BSD sendmsg approach.
** If the msghdr struct has a msg_accrights field then this is the one
** to use.
*/

#ifdef OLD_BSD_MSG

int brokerSendFD(sockfd, fd)
	int	sockfd;
	int	fd;
{
	struct iovec	iov[1];
	struct msghdr	msg;
	char		buf[1];

	iov[0].iov_base = (char *) buf;
	iov[0].iov_len  = 1;
	msg.msg_iov          = iov;
	msg.msg_iovlen       = 1;
	msg.msg_name         = (caddr_t) 0;
	msg.msg_accrights    = (caddr_t) &fd;
	msg.msg_accrightslen = sizeof(fd);

	if (sendmsg(sockfd, &msg, 0) < 0)
		return( (errno > 0) ? errno : 255 );

	return(0);
}

int brokerRecvFD(sockfd)
	int	sockfd;
{
	int		fd;
	struct iovec	iov[1];
	struct msghdr	msg;
	char		buf[1];

	iov[0].iov_base = (char *) buf;	
	iov[0].iov_len  = 1;
	msg.msg_iov          = iov;
	msg.msg_iovlen       = 1;
	msg.msg_name         = (caddr_t) 0;
	msg.msg_accrights    = (caddr_t) &fd;
	msg.msg_accrightslen = sizeof(fd);

	if (recvmsg(sockfd, &msg, 0) < 0)
		return(-1);

	return(fd);
}

#endif /* OLD_BSD_MSG */



/************************************************************************
** File descriptor passing routines for the new BSD sendmsg approach.
** If the msghdr struct has a msg_control field then this is the one
** to use.
*/

#ifdef NEW_BSD_MSG

int brokerSendFD(sockfd, fd)
	int	sockfd;
	int	fd;
{
	struct msghdr	msg;
	struct iovec	iov;
	struct cmsghdr	*cmsg;
	char		buf[2];
	union {
		struct cmsghdr 	cmsghdr;
		char		control[CMSG_SPACE(sizeof (int))];
	} cmsgu;

	bzero(&iov, sizeof(iov));
	iov.iov_base = buf;
	iov.iov_len = 2;

	bzero(&msg, sizeof(msg));
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	msg.msg_control = cmsgu.control;
	msg.msg_controllen = sizeof(cmsgu.control);
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_len = CMSG_LEN(sizeof (int));
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	*((int *) CMSG_DATA(cmsg)) = fd;

	if (sendmsg(sockfd, &msg, 0) != 2)
	{
		perror("brokerSendFD");
		return( (errno > 0) ? errno : 255 );
	}
	return(0);
}


int brokerRecvFD(sockfd)
        int     sockfd;
{
	struct msghdr   msg;
	struct iovec    iov;
	struct cmsghdr  *cmsg;
	char		buf[2];
	int		fd;
	union {
		struct cmsghdr  cmsghdr;
		char        control[CMSG_SPACE(sizeof (int))];
	} cmsgu;

	iov.iov_base = buf;
	iov.iov_len = 2;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cmsgu.control;
	msg.msg_controllen = sizeof(cmsgu.control);
	if (recvmsg(sockfd, &msg, 0) != 2)
	{
		perror("brokerRecvFD");
		return(-1);
	}

	cmsg = CMSG_FIRSTHDR(&msg);
	if (cmsg && cmsg->cmsg_len != CMSG_LEN(sizeof(int))) 
	{
		return(-1);
	}
	if (cmsg->cmsg_level != SOL_SOCKET || 
		cmsg->cmsg_type != SCM_RIGHTS)
	{
		return(-1);
	}
	fd = *((int *) CMSG_DATA(cmsg));
	return(fd);
}


#endif

#ifdef NEW_BSD_MSG_ORIG

#define CONTMSGLEN (sizeof(struct cmsghdr) + sizeof(int*))

int brokerSendFD(sockfd, fd)
	int	sockfd;
	int	fd;
{
	struct iovec	iov[1];
	struct msghdr	msg;
	char		buf[2];

	struct cmsghdr	*control;
	static char	*contbuf = NULL;

	iov[0].iov_base = (char *) buf;
	iov[0].iov_len  = 2;
	msg.msg_iov       = iov;
	msg.msg_iovlen    = 1;
	msg.msg_name      = (caddr_t) 0;
	msg.msg_namelen   = 0;

	if (!contbuf)
		contbuf = (char *)malloc(CONTMSGLEN);
	control = (struct cmsghdr *)contbuf;
	control->cmsg_level = SOL_SOCKET;
	control->cmsg_type = SCM_RIGHTS;
	control->cmsg_len = CONTMSGLEN;

	msg.msg_control   = (caddr_t) control;
	msg.msg_controllen = CONTMSGLEN;
	*(int *)CMSG_DATA(control) = fd;

	if (sendmsg(sockfd, &msg, 0) != 2)
	{
		perror("brokerSendFD");
		return( (errno > 0) ? errno : 255 );
	}

	return(0);
}

int brokerRecvFD(sockfd)
	int	sockfd;
{
	int		fd;
	struct iovec	iov[1];
	struct msghdr	msg;
	struct cmsghdr	*control;
	char		buf[2];
	static char	*contbuf = NULL;

	iov[0].iov_base = (char *) buf;	
	iov[0].iov_len  = 2;

	if (!contbuf)
		contbuf = (char *)malloc(CONTMSGLEN);
	bzero(contbuf,CONTMSGLEN);
	control = (struct cmsghdr *)contbuf;
	msg.msg_iov       = iov;
	msg.msg_iovlen    = 1;
	msg.msg_name      = (caddr_t) 0;
	msg.msg_namelen   = 0;
	msg.msg_control   = (caddr_t) control;
	msg.msg_controllen = CONTMSGLEN;


	if (recvmsg(sockfd, &msg, 0) != 2)
	{
		perror("brokerRecvFD");
		return(-1);
	}

	fd = *(int *)CMSG_DATA(control);
	return(fd);
}

#endif /* NEW_BSD_MSG */



/************************************************************************
** File descriptor passing routines for the SysV ioctl approach.
*/


#ifdef SYSV_MSG

int brokerSendFD(clifd, fd)
	int 	clifd, 
		fd;
{
    	char buf[2];

    	buf[0] = 0;
    	if (fd < 0) 
	{
        	buf[1] = -fd;
        	if (buf[1] == 0)
            		buf[1] = 1;
    	} 
	else 
	{
        	buf[1] = 0;
    	}

    	if (write(clifd, buf, 2) != 2) 
	{
        	fprintf(stderr,"pass_fd: write failed\n");
        	perror("pass_fd");
        	return -1;
    	}
    	if (fd >= 0)
	{
        	if (ioctl(clifd, I_SENDFD, fd) < 0) 
		{
            		fprintf(stderr,"pass_fd: ioctl failed\n");
            		perror("ioctl");
            		return -1;
        	}
	}
    	return 0;
} 

int brokerRecvFD(servfd) 
	int	servfd;
{
    	int  newfd, nread, flag, status;
    	char  *ptr, buf[IOBUFSIZE];
    	struct strbuf dat;
    	struct strrecvfd recvfd;

    	status = -1;
    	for ( ; ; ) 
	{
        	dat.buf = buf;
        	dat.maxlen = IOBUFSIZE;
        	flag = 0;
        	if (getmsg(servfd, NULL, &dat, &flag) < 0) 
		{
            		perror("getmsg");
            		exit(1);
        	}
        	nread = dat.len;
        	if (nread == 0) 
		{
            		fprintf(stderr,"httpd: connection closed by server\n");
            		exit(1);
        	}
        
        	for (ptr = buf; ptr < &buf[nread]; ) 
		{
            		if (*ptr++ == 0) 	
			{
                		if (ptr != &buf[nread-1]) 
				{
                    			perror("brokerRecvFD");
                    			exit(1);
                		}
                		status = *ptr & 255;
                		if (status == 0) 
				{
                    			if(ioctl(servfd, I_RECVFD, &recvfd) < 0)
                        			return -1;
                    			newfd = recvfd.fd;
                		} 
				else 
				{
                    			newfd = -status;
				}
                		nread -= 2;
            		}
        	}
        	if (status >= 0)
            		return (newfd);
    	}
} 

#endif



/************************************************************************
** File descriptor passing routines for the Linux /proc approach.
*/


#ifdef LINUX_MSG

int brokerSendFD(int clifd, int fd) 
{
	char	path[255];
	static uid_t	uid = -1;
	static gid_t	gid;

	if (uid == -1)
	{
		uid = geteuid();
		gid = getgid();
	}
	sprintf(path,"/proc/self/fd/%d",fd);
	chown(path,uid,gid);
    	if (write(clifd, &fd, sizeof(fd)) != sizeof(fd)) 
	{
        	fprintf(stderr,"pass_fd: write failed\n");
        	perror("pass_fd");
		return -1;
    	}
    	return 0;
} 

int brokerRecvFD(int servfd) 
{
    	int  	newfd;
	char	path[255];
	static	int ppid = 0;

    	read(servfd, &newfd, sizeof(newfd));
	if (ppid == 0)
		ppid = getppid();
	sprintf(path,"/proc/%d/fd/%d",ppid,newfd);
	newfd = open(path,O_RDWR,0);
	return(newfd);
}
#endif

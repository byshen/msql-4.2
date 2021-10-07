/*
** filepass.c	: File descriptor passing test
*/

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/param.h>


#ifdef OLD_BSD_MSG

int main()
{
	struct iovec	iov[1];
	struct msghdr	msg;
	extern int	errno;
	char		buf[1];
	int		fd;

	iov[0].iov_base = (char *) buf;
	iov[0].iov_len  = 1;
	msg.msg_iov          = iov;
	msg.msg_iovlen       = 1;
	msg.msg_name         = (caddr_t) 0;
	msg.msg_accrights    = (caddr_t) &fd;
	msg.msg_accrightslen = sizeof(fd);
}
#endif /* OLD_BSD_MSG */


#ifdef NEW_BSD_MSG

#define CONTMSGLEN sizeof(struct cmsghdr) + sizeof(int)

int main()
{
	struct iovec	iov[1];
	struct msghdr	msg;
	struct cmsghdr	*control;
	extern int	errno;
	char		buf[1];
	static char	*contbuf = NULL;
	int		fd;

	iov[0].iov_base = (char *) buf;
	iov[0].iov_len  = 1;

	if (!contbuf)
		contbuf = (char *)malloc(CONTMSGLEN);
	control = (struct cmsghdr *)contbuf;
	control->cmsg_len = CONTMSGLEN;
	control->cmsg_level = SOL_SOCKET;
	control->cmsg_type = SCM_RIGHTS;
	*(int *)CMSG_DATA(control) = fd;
	msg.msg_iov       = iov;
	msg.msg_iovlen    = 1;
	msg.msg_name      = (caddr_t) 0;
	msg.msg_control   = (caddr_t) control;
	msg.msg_controllen = CONTMSGLEN;
}
#endif /* NEW_BSD_MSG */



#ifdef SYSV_MSG

int main()
{
    	char 	buf[2];
	int	fd;

	ioctl(fd, I_SENDFD, fd);
} 
#endif


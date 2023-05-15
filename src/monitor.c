/*
  +----------------------------------------------------------------------+
  | pmmpthread                                                             |
  +----------------------------------------------------------------------+
  | Copyright (c) Joe Watkins 2015                                       |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Joe Watkins <krakjoe@php.net>                                |
  +----------------------------------------------------------------------+
 */

#include <src/pmmpthread.h>
#include <src/monitor.h>

zend_result pmmpthread_monitor_init(pmmpthread_monitor_t* m) {
	pthread_mutexattr_t at;

	m->state = 0;

	pthread_mutexattr_init(&at);
#if defined(PTHREAD_MUTEX_RECURSIVE) || defined(__FreeBSD__)
	pthread_mutexattr_settype(&at, PTHREAD_MUTEX_RECURSIVE);
#else
	pthread_mutexattr_settype(&at, PTHREAD_MUTEX_RECURSIVE_NP);
#endif
	int ret = pthread_mutex_init(&m->mutex, &at);
	pthread_mutexattr_destroy(&at);
	if (ret != 0) {
		free(m);
		return FAILURE;
	}

	if (pthread_cond_init(&m->cond, NULL) != 0) {
		pthread_mutex_destroy(&m->mutex);
		free(m);
		return FAILURE;
	}

	return SUCCESS;
}

void pmmpthread_monitor_destroy(pmmpthread_monitor_t* m) {
	pthread_mutex_destroy(&m->mutex);
	pthread_cond_destroy(&m->cond);
}

zend_bool pmmpthread_monitor_lock(pmmpthread_monitor_t *m) {
	return (pthread_mutex_lock(&m->mutex) == 0);
}

zend_bool pmmpthread_monitor_unlock(pmmpthread_monitor_t *m) {
	return (pthread_mutex_unlock(&m->mutex) == 0);
}

pmmpthread_monitor_state_t pmmpthread_monitor_check(pmmpthread_monitor_t *m, pmmpthread_monitor_state_t state) {
	return (m->state & state);
}

int pmmpthread_monitor_wait(pmmpthread_monitor_t *m, long timeout) {
	struct timeval time;
	struct timespec spec;

	if (timeout == 0) {
		return pthread_cond_wait(&m->cond, &m->mutex);
	}

	if (gettimeofday(&time, NULL) != 0) {
		return -1;
	}

	time.tv_sec += (timeout / 1000000L);
	time.tv_sec += (time.tv_usec + (timeout % 1000000L)) / 1000000L;
	time.tv_usec = (time.tv_usec + (timeout % 1000000L)) % 1000000L;

	spec.tv_sec = time.tv_sec;
	spec.tv_nsec = time.tv_usec * 1000;

	return pthread_cond_timedwait(&m->cond, &m->mutex, &spec);
}

int pmmpthread_monitor_notify(pmmpthread_monitor_t *m) {
	return pthread_cond_broadcast(&m->cond);
}

int pmmpthread_monitor_notify_one(pmmpthread_monitor_t *m) {
	return pthread_cond_signal(&m->cond);
}

void pmmpthread_monitor_wait_until(pmmpthread_monitor_t *m, pmmpthread_monitor_state_t state) {
	if (pmmpthread_monitor_lock(m)) {
		while (!pmmpthread_monitor_check(m, state)) {
			if (pmmpthread_monitor_wait(m, 0) != 0) {
				break;
			}
		}
		pmmpthread_monitor_unlock(m);
	}
}

void pmmpthread_monitor_add(pmmpthread_monitor_t *m, pmmpthread_monitor_state_t state) {
	if (pmmpthread_monitor_lock(m)) {
		m->state |= state;
		pmmpthread_monitor_notify(m);
		pmmpthread_monitor_unlock(m);
	}
}

void pmmpthread_monitor_remove(pmmpthread_monitor_t *m, pmmpthread_monitor_state_t state) {
	if (pmmpthread_monitor_lock(m)) {
		m->state &= ~state;
		pmmpthread_monitor_notify(m);
		pmmpthread_monitor_unlock(m);
	}
}

/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_ls.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kkaiyawo <kkaiyawo@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/24 17:30:55 by kkaiyawo          #+#    #+#             */
/*   Updated: 2026/05/05 10:42:08 by kkaiyawo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../../include/ft_ls.h"

/**
 * returns 0 and set flag to true if match, returns 1 if not match
 */
int toggle_opt(const char o) {
	if (o == 'a')
		g_opts.all = 1;
	else if (o == 'l')
		g_opts.long_format = 1;
	else if (o == 'R')
		g_opts.recursive = 1;
	else if (o == 'r')
		g_opts.reverse_order = 1;
	else if (o == 't')
		g_opts.time_sort = 1;
	else
		return (1);
	return (0);
}

/**
 *
 */
int render_flags(char *arg) {
	size_t i;

	i = 0;
	if (arg[i++] != '-')
		return (0);
	if (arg[i] == '-') {
		if (arg[++i] == '\0')
			return (0);
	} else {
		while (arg[i]) {
			if (toggle_opt(arg[i++]) > 0)
				return (EINVAL);
		}
	}
	return (1);
}

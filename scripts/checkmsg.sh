#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2023 Telechips Inc.

function check_cherry_pick_info()
{
	local cherrypick_msg="^(cherry picked from commit .*)$"

	if [ -z "$(echo "${body}" | grep "${cherrypick_msg}")" ]; then
		echo "${refs}: Cherry-pick info not specified"
		err=1
	fi
}

function check_message_form()
{
	if [ -n "$(echo "${message}" | sed -n "2p")" ]; then
		echo "${refs}: Multi-line subject"
		err=1
	fi
}

function check_patch_type()
{
	local patch_types="^\[\(fea\|upg\|ref\|bug\)\]"

	if [ -z "$(echo "${subject}" | grep "${patch_types}")" ]; then
		echo "${refs}: Patch type not specified"
		err=1
	fi
}

function check_subsys_classifier()
{
	if [ -z "$(echo "${subject}" | grep "^.*:")" ]; then
		echo "${refs}: Subsystem classifier not specified"
		err=1
	fi
}

function check_body_width()
{
	if [ "$(echo "${body}" | wc -L)" -gt 72 ]; then
		echo "${refs}: Message width exceeded (> 72 columns)"
		err=1
	fi
}

function check_trailer()
{
	if [ -n "$(echo "${body}" | grep "^Change-Id:")" ]; then
		echo "${refs}: Deprecated trailer specified (Change-Id)"
		err=1
	fi

	if [ -z "$(echo "${body}" | grep "^TCS: .\+")" ]; then
		echo "${refs}: TCS issue trailer not specified"
		err=1
	fi
}

if [ $# != 1 ]; then
	echo "Usage: $0 <ref>"
	exit 1
fi

err=0

merge_base=$(git merge-base $1 HEAD)
hashes=($(git log --pretty=%h --abbrev=7 ${merge_base}..HEAD))

for refs in ${hashes[@]}; do
	author=$(git show --no-patch --pretty=%ae ${refs})
	message=$(git show --no-patch --pretty=%B ${refs})
	subject=$(git show --no-patch --pretty=%s ${refs})
	body=$(git show --no-patch --pretty=%b ${refs})

	if [ -z "$(echo "${author}" | grep "telechips\.com$")" ]; then
		# For cherry-picks from open source
		check_cherry_pick_info
		check_message_form
	elif [ -n "$(git show --merges ${refs})" ]; then
		#For merge commits
		check_message_form
		check_trailer
	elif [ -n "$(echo "${subject}" | grep "^Revert ")" ]; then
		#For revert commits
		check_message_form
		check_body_width
		check_trailer
	else
		#For general commits
		check_message_form
		check_patch_type
		check_subsys_classifier
		check_body_width
		check_trailer
	fi
done

exit ${err}

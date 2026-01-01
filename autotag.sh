#!/bin/bash

# This script tags the state of the current branch and optionally pushes
# tags to the remote.

echo "$0 $@"

os=$(uname -s)
if [[ "$os" == "Darwin" ]]; then
    date_arg="-r "
    stat_arg="-f %m"
else
    date_arg="--date=@"
    stat_arg="-c %Y"
fi

time=`TZ=America/Detroit date +%s`
date=`TZ=America/Detroit date ${date_arg}${time} +%Y.%m.%d_%H.%M.%S`

# Get top level of working tree (also make sure I'm in a git repository)
top=`git rev-parse --show-toplevel`
if [[ $? -ne 0 ]]; then
    echo "Error: Not in a git repository.  Please run autotag.sh from a git repository."
    exit 1
fi

cd "$top"

# Make sure the specified files are in the git repository.
# Also find last modification time among files that have been locally modified.
push=0
modified_file=""
for arg in "$@"; do
    if [[ "$arg" = "push" ]]; then
        push=1
    elif [[ ! -f "$arg" ]]; then
        echo "Error: $arg does not exist"
        exit 1
    else
        git ls-files --error-unmatch "$arg" > /dev/null 2>&1
        if [[ $? -ne 0 ]]; then
            echo "Error: $arg has not been added to the git repository"
            exit 1
        fi
	git status --porcelain "$arg" | grep -E '^( M|AM|M)' > /dev/null 2>&1
	if [[ $? -eq 0 ]]; then
	    m=$((`stat ${stat_arg} "$arg"`-60)) # reduce age of file by 1 minute
	    if [[ ${modified_file} == "" || ${m} -gt ${modified} ]]; then
		modified=${m}
		modified_file=${arg}
	    fi
	fi
    fi
done

# Find time of last edit
edit=`tail -q -n 1 .version482/* 2> /dev/null | sed 's/^[^ ]* \([0-9]*\).*/\1/' | sort -n | tail -n 1`
if [[ "$edit" != "" ]]; then
    edit=',edit-'`TZ=America/Detroit date ${date_arg}${edit} +%Y.%m.%d_%H.%M.%S`
fi

# Ignore signals, so users don't stop in an intermediate state
trap "" SIGINT SIGQUIT SIGTSTP

# Make sure this git repository has at least one commit, so HEAD~ refers to
# something when undoing the first temporary commit
out=`git log > /dev/null 2>&1`
if [[ $? -ne 0 ]]; then
    git commit -m "initial commit" --allow-empty > /dev/null
fi

# Create first temporary commit
git commit -m compile-${date}-tmp1 --allow-empty > /dev/null
if [[ $? -eq 0 ]]; then

    # Add version files to repo (only for the second temporary commit)
    git add -f ".version482" > /dev/null 2>&1

    # Create second temporary commit
    git commit -am compile-${date}-tmp2 --allow-empty > /dev/null

    if [[ $? -eq 0 ]]; then
        # create compile/edit tag for the second temporary commit
        git tag -a compile-${date}${edit} -m "" > /dev/null 2>&1

	if [[ $? -eq 0 ]]; then
	    # Remove version482 files that are guaranteed not to change
	    shopt -s nullglob
	    for file in .version482/*; do
		t=`basename "$file" | sed 's/\..*//'`
		if [[ $((time-t)) -gt 3600 ]]; then
		    rm "$file"
		fi
	    done
	    shopt -u nullglob
	fi

        # Undo second temporary commit.  Use --mixed to unstage the files
        # that were staged because of the -a argument to git commit
        git reset --mixed HEAD~ > /dev/null

        # Push tag (if requested on the command line).  Allow this to be killed
        # (without killing autotag.sh).
        if [[ $push -eq 1 ]]; then
            # disallow manual typing of ssh passphrase
            export SSH_ASKPASS=echo
            export SSH_ASKPASS_REQUIRE=force
            (trap - SIGINT SIGQUIT SIGTSTP; git push --tags --quiet)
        fi
    fi

    # Undo first temporary commit.  Use --soft to preserve the files that
    # that were already staged before running autotag.sh
    git reset --soft HEAD~ > /dev/null

fi

# Make sure version482 is up-to-date
min="20250913"
version=`tail -q -n 1 .version482/* 2> /dev/null | sed 's/ .*//' | sed 's/^.*-//' | sort | tail -n 1`
if [[ ${version} != "" && ${version} < ${min} ]]; then
    echo "*** Please update your installed version482 extension to ${min} or higher. ***"
fi

# Make sure versions are being kept
if [[ ${modified_file} != "" ]]; then
    edit_date=`git tag | grep 'edit-[0-9][0-9][0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]_[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]' | sed 's/^.*edit-//' | sort | tail -n 1`
    modified_date=`TZ=America/Detroit date ${date_arg}${modified} +%Y.%m.%d_%H.%M.%S`
    if [[ ${edit_date} < ${modified_date} ]]; then
	echo "Error: ${modified_file} is newer than local version482 history.  Please edit your code with Visual Studio Code or Neovim, with the version482 extension enabled."
	exit 1
    fi
fi

exit 0

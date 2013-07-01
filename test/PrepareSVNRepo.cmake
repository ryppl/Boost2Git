find_program(SVN NAMES svn)
find_program(SVNADMIN NAMES svnadmin)

set(REPO_PATH "${CMAKE_CURRENT_BINARY_DIR}/test-repo")
set(REPO_URI "file://${REPO_PATH}")
set(WC_PATH "${CMAKE_CURRENT_BINARY_DIR}/test-wc")

set(LOG_MSG --username test -m)

file(REMOVE_RECURSE "${REPO_PATH}" "${WC_PATH}")

function(launch)
  execute_process(COMMAND ${ARGV} RESULT_VARIABLE result_code)
  if(NOT result_code STREQUAL 0)
    message(FATAL_ERROR "${ARGV}:\n" 
      "Process failed with result \"${result_code}\"")
  endif()
endfunction()

# Prepare the basic repository structure
launch("${SVNADMIN}" create ${REPO_PATH})
# r1,2,3: create trunk/, tags/, and branches/
launch(${SVN} mkdir ${LOG_MSG} "create trunk/" ${REPO_URI}/trunk)
launch(${SVN} mkdir ${LOG_MSG} "create tags/" ${REPO_URI}/tags)
launch(${SVN} mkdir ${LOG_MSG} "create branches/" ${REPO_URI}/branches)

# Create a working copy
launch("${SVN}" checkout "${REPO_URI}/trunk" test-wc)

# A function for executing svn commands in the working directory
function(svn)
  launch(${SVN} ${ARGV} WORKING_DIRECTORY ${WC_PATH})
endfunction()

# r4: check in a README
file(WRITE "${WC_PATH}/README.txt" "This is the README")
svn(add README.txt)
svn(commit ${LOG_MSG} "first commit")

# r5: modify README
file(WRITE "${WC_PATH}/README.txt" "This is the new README")
svn(commit ${LOG_MSG} "updated README")

# r6,7: create a branch and a tag
svn(cp ${LOG_MSG} "create branch" "${REPO_URI}/trunk" "${REPO_URI}/branches/branch1")
svn(cp ${LOG_MSG} "create tag" "${REPO_URI}/trunk" "${REPO_URI}/tags/tag1")

# r8: commit on the branch
svn(switch "${REPO_URI}/branches/branch1" .)
file(WRITE "${WC_PATH}/READYOU.txt" "This is the NEW YOU")
svn(add READYOU.txt)
svn(commit ${LOG_MSG} "added READYOU")

# r9: update the trunk
svn(switch "${REPO_URI}/trunk" .)
file(WRITE "${WC_PATH}/README.txt" "This is the Final README")
svn(commit ${LOG_MSG} "Final README")



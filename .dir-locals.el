;;; Directory Local Variables
;;; See Info node `(emacs) Directory Variables' for more information.

((nil
  (eval progn
        (setq-local grep-command "git grep -nH -e")
        (c-add-style "boost2git"
                     '((c-basic-offset . 2)
                       (c-offsets-alist
                        (access-label . -)
                        (arglist-cont . 0)
                        (arglist-intro . ++)
                        (block-close . 0)
                        (brace-list-close . 0)
                        (brace-list-entry . 0)
                        (brace-list-intro . 0)
                        (brace-list-open . +)
                        (catch-clause . 0)
                        (class-close . +)
                        (class-open . +)
                        (defun-block-intro . 0)
                        (defun-close . 0)
                        (defun-open . +)
                        (else-clause . 0)
                        (inclass lambda
                                 (context)
                                 (save-excursion
                                   (goto-char
                                    (cdr context))
                                   (let
                                       ((context
                                         (c-guess-basic-syntax)))
                                     (if
                                         (and
                                          (eq
                                           (caar context)
                                           'class-open)
                                          (progn
                                            (goto-char
                                             (cadar context))
                                            (when
                                                (looking-at "template")
                                              (c-forward-token-1)
                                              (c-forward-sexp)
                                              (c-forward-sws))
                                            (looking-at "struct")))
                                         0 '+))))
                        (inline-close . 0)
                        (innamespace . 0)
                        (label . -)
                        (member-init-cont . -)
                        (member-init-intro . ++)
                        (namespace-close . 0)
                        (namespace-open . 0)
                        (statement . 0)
                        (statement-block-intro lambda
                                               (context)
                                               (save-excursion
                                                 (beginning-of-line)
                                                 (c-lineup-whitesmith-in-block context)))
                        (statement-cont lambda
                                        (context)
                                        (save-excursion
                                          (beginning-of-line)
                                          (c-forward-sws)
                                          (if
                                              (looking-at "\\([-+%*^&/|~]\\|&&|||\\)=")
                                              '* '+)))
                        (stream-op . c-lineup-streamop)
                        (substatement . ++)
                        (substatement-open . +)
                        (topmost-intro . 0)
                        (topmost-intro-cont . 0)
                        (annotation-top-cont . 0)
                        (annotation-var-cont . +)
                        (arglist-close . +)
                        (arglist-cont-nonempty . +)
                        (block-open . 0)
                        (brace-entry-open . 0)
                        (c . c-lineup-C-comments)
                        (case-label . 0)
                        (comment-intro . c-lineup-comment)
                        (composition-close . 0)
                        (composition-open . 0)
                        (cpp-define-intro c-lineup-cpp-define +)
                        (cpp-macro . -1000)
                        (cpp-macro-cont . +)
                        (do-while-closure . 0)
                        (extern-lang-close . 0)
                        (extern-lang-open . 0)
                        (friend . 0)
                        (func-decl-cont . +)
                        (incomposition . +)
                        (inexpr-class . +)
                        (inexpr-statement . +)
                        (inextern-lang . +)
                        (inher-cont . c-lineup-multi-inher)
                        (inher-intro . +)
                        (inlambda . c-lineup-inexpr-block)
                        (inline-open . +)
                        (inmodule . +)
                        (knr-argdecl . 0)
                        (knr-argdecl-intro . +)
                        (lambda-intro-cont . +)
                        (module-close . 0)
                        (module-open . 0)
                        (objc-method-args-cont . c-lineup-ObjC-method-args)
                        (objc-method-call-cont c-lineup-ObjC-method-call-colons c-lineup-ObjC-method-call +)
                        (objc-method-intro .
                                           [0])
                        (statement-case-intro . +)
                        (statement-case-open . 0)
                        (string . -1000)
                        (substatement-label . 2)
                        (template-args-cont c-lineup-template-args +))))))
 (c++-mode
;  (c-file-style . "boost2git")
  )
 (c-mode
;  (c-file-style . "boost2git")
))

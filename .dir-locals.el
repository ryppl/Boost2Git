;;; Directory Local Variables
;;; See Info node `(emacs) Directory Variables' for more information.

((nil . ((eval . 
               (c-add-style "boost2git"
             '(;"*c-guess*:/tmp/boost2git.cpp"
               (c-basic-offset . 2)     ; Guessed value
               (c-offsets-alist
                (access-label . -)      ; Guessed value
                (arglist-cont . 0)      ; Guessed value
                (arglist-intro . ++)    ; Guessed value
                (block-close . 0)       ; Guessed value
                (brace-list-close . 0)  ; Guessed value
                (brace-list-entry . 0)  ; Guessed value
                (brace-list-intro . 0)  ; Guessed value
                (brace-list-open . +)   ; Guessed value
                (catch-clause . 0)      ; Guessed value
                (class-close . +)       ; Guessed value
                (class-open . +)
                (defun-block-intro . 0) ; Guessed value
                (defun-close . 0)       ; Guessed value
                (defun-open . +)        ; Guessed value
                (else-clause . 0)       ; Guessed value
                (inclass
                 . (lambda (context)
                     (save-excursion
                       (goto-char (cdr context))
                       (let ((context (c-guess-basic-syntax)))
                         (if (and (eq (caar context) 'class-open)
                                  (progn (goto-char (cadar context))
                                         (when (looking-at "template")
                                             (c-forward-token-1)
                                             (c-forward-sexp)
                                             (c-forward-sws))
                                         (looking-at "struct")))
                             0 '+))))
                 )
                (inline-close . 0)       ; Guessed value
                (innamespace . 0)        ; Guessed value
                (label . -)              ; Guessed value
                (member-init-cont . -)  ; Guessed value
                (member-init-intro . ++) ; Guessed value
                (namespace-close . 0)    ; Guessed value
                (namespace-open . 0)     ; Guessed value
                (statement . 0)          ; Guessed value
                (statement-block-intro 
                 . (lambda (context)
                     (save-excursion
                       (beginning-of-line)
                       (c-lineup-whitesmith-in-block context)))
                 )
                (statement-cont 
                 . (lambda (context)
                     (save-excursion
                       (beginning-of-line)
                       (c-forward-sws)
                       (if (looking-at "\\([-+%*^&/|~]\\|&&\|||\\)=")
                           '* '+))))
                (stream-op . c-lineup-streamop)     ; Guessed value
                (substatement . ++)     ; Guessed value
                (substatement-open . +) ; Guessed value
                (topmost-intro . 0)     ; Guessed value
                (topmost-intro-cont . 0) ; Guessed value

;; unguessed values follow

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
                (template-args-cont c-lineup-template-args +))))

               )))

(c-mode
  (c-file-style . "boost2git"))
(c++-mode
  (c-file-style . "boost2git")))







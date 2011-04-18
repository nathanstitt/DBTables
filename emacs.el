;;; $Id: emacs.el 677 2005-03-18 20:31:55Z nas $

(defun my-save-and-compile ()
  (interactive "")
  (save-buffer 0)
  (compile "make -C /usr/local/lib/site_ruby/nas/dbtables && ~/code/ruby/lib/t/dbtables.rb"))
(global-set-key "\C-c\C-v" 'my-save-and-compile)

(defun my-save-and-compile ()
  (interactive "")
  (save-buffer 0)
  (compile "make -C /usr/local/lib/site_ruby/nas/dbtables"))

(defun my-save-and-compile ()
  (interactive "")
  (save-buffer 0)
  (compile "make -C /usr/local/lib/site_ruby/nas/dbtables && ~/code/wh_upgrade/so-printer.rb /tmp/so.txt nas"))

(defun my-save-and-compile ()
  (interactive "")
  (save-buffer 0)
  (compile "make -C /usr/local/lib/site_ruby/nas/dbtables && ~/code/ruby/lib/t/dbtables.rb"))

;;; end of emacs.el

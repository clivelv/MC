
(defun f (x y)
  (match (cons x y)
         ((1 . 2) 'a-symbol)))

(eq (f 1 2) 'a-symbol)

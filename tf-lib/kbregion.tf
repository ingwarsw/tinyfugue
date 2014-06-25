;;; cut/copy/paste input region
;;; this file is incomplete

/loaded __TFLIB__/kbregion.tf

/def -ib'^x@'	= /kb_set_mark
/def -ib'^xx'	= /kb_cut_region
/def -ib'^x<'	= /kb_copy_region
/def -ib'^x>'	= /kb_paste_buffer
/def -ib'^x^x'	= /kb_exchange_point_and_mark

/set _kb_mark=-1

/def -i kb_set_mark = \
    /set _kb_mark=$[kbpoint()]%;\
    /echo -e %% First mark set at position %{_kb_mark}.

/def -i kb_exchange_point_and_mark = \
    /if ( _kb_mark > 0 ) \
        /let _point=$[kbpoint()]%;\
        /@test kbgoto(_kb_mark)%;\
        /set _kb_mark=%_point%;\
    /else \
        /echo -e %% Mark not set.%;\
    /endif%;\

/def -i kb_copy_region = \
    /if ( _kb_mark > 0 ) \
        /set _kb_region=%;\
        /if ( _kb_mark > kbpoint() ) \
            /@test _kb_region := substr(kbtail(), 0, kbpoint() - _kb_mark)%;\
        /else \
            /@test _kb_region := substr(kbhead(), _kb_mark, kbpoint())%;\
        /endif%;\
        /echo -e %% Region copied into buffer.%;\
        /set _kb_mark=-1%;\
    /else \
        /echo -e %% Mark not set.%;\
    /endif%;\

/def -i kb_cut_region = \
    /if ( _kb_mark > 0 ) \
        /let _mark=%{_kb_mark}%;\
        /kb_copy_region%;\
        /@test kbdel(_mark)%;\
    /endif

/def -i kb_paste_buffer = \
    /@test input(_kb_region)


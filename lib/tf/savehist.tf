/set savehist_version=$Id: savehist.tf,v 35000.5 2004/08/04 18:31:51 hawkeye Exp $
;;;; Save/restore world histories

;;;; Typical usage in .tfrc:
; /addworld ...
; ...
; /set savehist_dir=~/tf-hist
; /load_histories
; /repeat -0:10 i /save_histories


/loaded __TFLIB__/savehist.tf

/if (systype() !~ "unix") \
    /echo -e %% Warning: savehist.tf may not work on non-unix systems.%; \
/endif

/require textutil.tf
/require textencode.tf
/require lisp.tf

/if (savehist_dir =~ "") /set savehist_dir=~/tf-hist%; /endif

;; /save_histories [-lig]
;; saves all world histories, and optionally local/input/global.  If none of
;; -lig are given, -i is assumed.
/def -i save_histories = \
    /if (!getopts("lig", 0)) /return 0%; /endif%; \
    /if (!opt_l | !opt_i | !opt_g) /let opt_i=1%; /endif%; \
;   %savehist may appear on status bar
    /set savehist=H%; \
    /if (opt_l) /save_history -l%; /endif%; \
    /if (opt_i) /save_history -i%; /endif%; \
    /if (opt_g) /save_history -g%; /endif%; \
;   (unnamedN) worlds won't show up without /listworlds -u
    /quote -S /~save_histories `/listworlds -s%; \
    /unset savehist

/def -i ~save_histories = \
;   /_echo %1%; \
    /save_history -w%1

/def -i load_histories = \
    /echo -e %% Loading all histories...%; \
;   /let old_status_fields=%status_fields%; \
;   /set _savehist=%; \
;   /set status_fields=$[replace("@world", "_savehist", status_fields)]%; \
    /set _loaded_histories=%; \
    /let _histories=$(/quote -S -decho !\
	cd $[filename(savehist_dir)] && echo `ls -1 l i g w* 2>/dev/null`)%; \
    /if (_histories =~ "") \
	/echo -e %% %0: no history files found in "%savehist_dir".%; \
    /else \
	/mapcar /~load_histories %_histories%; \
	/echo -e %% Loaded histories: %_loaded_histories%; \
    /endif%; \
;    /set status_fields=%old_status_fields%; \
    /unset _loaded_histories

/def -i ~load_histories = \
    /let ehist=%1%; \
    /let hist=$[textdecode(ehist)]%; \
;   /set _savehist=%hist%; \
    /let file=%savehist_dir/%ehist%; \
;   Compatibility with optional cchan.tf
    /if (regmatch("w(([^:]+):(.*))$", hist) & ismacro("cchan_addworld") & \
	world_info({P1}, "name") =~ "" & \
	world_info({P2}, "type") =/ "*.cchan.*") \
	    /cchan_addworld $(/listworlds -s %P2):%P3%; \
    /endif%; \
;    /echo -e - Loading history: %hist%; \
    /if /load_history -%hist%; /then \
	/set _loaded_histories=%_loaded_histories %hist%; \
    /endif

;; /set_savehist_last <ehist> <hist>
/def -i set_savehist_last = \
    /set savehist_last_%1=$[regmatch("^[0-9]+", $(/recall -%2 #/1)) & {P0}]

/def -i savehist_args = \
    /if (!regmatch("^-([lig]|w(.+))$", {-1})) \
	/echo -e %% usage: %1 {-l|-i|-g|-w<world>}%; \
	/return 0%; \
    /endif%; \
    /set savehist_hist=%P1%; \
    /let world=%P2%; \
    /if (world !~ "" & world_info(world, "name") =~ "") \
	/echo -e %% %1: no world %world%; \
	/return 0%; \
    /endif%; \
    /return 1

/def -i _hist_write = \
    /let _line=%; \
    /while (tfread('i', _line) >= 0) \
	/test tfwrite({1}, encode_attr(_line))%; \
    /done


; /save_history {-l|-i|-g|-w<world>}
/def -i save_history = \
    /if /!savehist_args %0 %*%; /then /return 0%; /endif%; \
    /let hist=%savehist_hist%; \
    /let ehist=$[textencode(hist)]%; \
    /let dir=$[filename(savehist_dir)]%; \
    /let file=%dir/%ehist%; \
    /let first=%; \
    /test first:=savehist_last_%ehist + 1%; \
    /recall -%hist %{first}- %| /let count=%?%; \
    /if (!count) /return 1%; /endif%; \
    /setenv _file=%file%; \
    /setenv _dir=%dir%; \
;   if appending to file would give it more than 10% extra lines, truncate it
    /let filesize=savehist_filesize_%ehist%; \
    /histsize -%hist %| /let max=%{?}%; \
    /if /test %{filesize}%; /then \
;	The filesize is cached, and we'll assume the directory exists, avoiding
;	potentially expensive shell calls.
	/test %{filesize} := %{filesize} + count%; \
    /else \
;	Filesize is unknown; this must be the first call to save_history for
;	this history.  We must make sure the dir exists and get the filesize.
	/set %{filesize}=0%; \
	/quote -S -decho %% !test -d $$_dir || mkdir $$_dir%; \
	/if ({?} != 0) \
	    /unset _dir%; \
	    /unset _file%; \
	    /return%; \
	/endif%; \
	/quote -S /test %{filesize}:=+"!"wc -l %_file""%; \
    /endif%; \
    /let mode=a%; \
    /if /test %{filesize} + count > max * 1.1%; /then \
	/if (count >= max) \
	    /test count := max%; \
	    /let mode=w%; \
	    /test %{filesize} := 0%; \
	/else \
	    /setenv _tmp=%_dir/tfhist-$[getpid()]%; \
	    /quote -S -decho %% !touch $$_tmp && chmod go-rwx $$_tmp && \
		tail -$[max-count] $$_file > $$_tmp && mv $$_tmp $$_file%; \
	    /if ({?} == 0) \
		/test %{filesize} := max - count%; \
	    /endif%; \
	    /unset _tmp%; \
	/endif%; \
    /endif%; \
;   append the new lines to the file
    /let old_max_iter=%max_iter%; \
    /set max_iter=0%; \
    /let out=%; \
    /if ((out := tfopen(_file, mode)) >= 0) \
	/if /test !%{filesize}%; /then \
	    /quote -S -decho %% !chmod go-rwx $$_file%; \
	/endif%; \
	/test tfflush(out, 0)%; \
;	"i" can't have attributes, so we don't bother encoding them.
	/let opts=-%hist /%count%; \
	/if (savehist_hist =~ "i") \
	    /quote -S -decho #-t'%%@ -' %opts %| /test copyio('i', out)%; \
	/else \
	    /quote -S -decho #-t'%%@ -p -' %opts %| /test _hist_write(out)%; \
	/endif%; \
	/test tfclose(out)%; \
	/set max_iter=%old_max_iter%; \
	/test %{filesize} := %{filesize} + count%; \
	/set_savehist_last %ehist %hist%; \
    /else \
;	Directory may be corrupt.  Forget cached filesize.
	/unset %{filesize}%; \
    /endif%; \
    /unset _dir%; \
    /unset _file

; /load_history {-l|-i|-g|-w<world>}
/def -i load_history = \
    /if /!savehist_args %0 %*%; /then /return 0%; /endif%; \
    /let file=%savehist_dir/$[textencode(savehist_hist)]%; \
    /quote -S /recordline -%savehist_hist -t'%file%; \
    /set_savehist_last %ehist %hist%; \
    /return 1


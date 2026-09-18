<h1><img src="https://fuze.page/static/fuze-min-hover.png" style="max-width: 100%;" width="94" height="45" alt="FUZE"> Http</h1>
<p>
FuzeHttp<sup>[<a href="https://github.com/Liteb0t/FuzeHttp">github</a>]</sup> is a web framework written in C++23, designed for modern REST API-based services.<br>
This is the engine driving <a href="https://fuze.page/software/mediaboard">Fuze Mediaboard</a> and is developed in tandem with it.
</p>
<h2>Building the example project</h2>
The easiest way to get started is to set up the example project, and then work from there. If you have deployed Fuze Mediaboard before, these steps will be very familiar.
<h3>Compiling from source</h3>
<p>
	Prerequisites: Git, CMake (>= 3.28), Boost (>= 1.88), SQLite OR PostgreSQL.<br><br>
	First ensure that submodules are downloaded. Use this command:<br>
	<code>git submodule update --init --recursive</code><br>
	Now, to compile the server:<br>
	<code>cmake -B build -G Ninja -D WITH_EXAMPLE=ON</code><br>
	<code>cmake --build build</code><br>
	<i>Compiling is known to work with clang-19, but not GCC 14.</i>
</p>
<h3>Running the example project</h3>
<p>
	This is the same procedure to create the owner account, as in Fuze Mediaboard.<br>
	If compiled from source, execute <code>./build/bin/example --create_owner</code>
	The <code>--create_owner</code> flag is required on first boot. When the server starts, an invitation link will appear in the command-line output.
	<figure>
		<img src="https://fuze.page/static/Fuze_Mediaboard_0.1.3_register_owner_account_link.png" alt="Registration link">
		<figcaption>Invitation link to create owner account.</figcaption>
	</figure>
	<figure>
		<img src="https://fuze.page/static/Fuze_Mediaboard_0.1.3_accept_invitation.png" alt="Accepting the invitation">
		<figcaption>A registration form is shown when invitation is opened</figcaption>
	</figure>
	After registering the owner account, you can now interact with the server.<br>
	<i>If you ever forget the password, you can simply run the create_owner command again. Note that it will not be the same account.</i>
</p>
<h2>Architecture</h2>
<p>
This framework has not been designed in advance. It is gradually being developed in an ad-hoc way, to meet the growing needs of Fuze Mediaboard. In this sense, this project is in the spirit of C++.
</p>
<h3>Configuration</h3>
<p>
Options can be defined in <code>config.ini</code> or passed in at runtime. Run the server with <code>--help</code> to see all available options. Some options are built-in, such as <code>threads</code> and <code>environment_variable_for_secret</code>.<br><br>
Additional options can be defined.
</p>
